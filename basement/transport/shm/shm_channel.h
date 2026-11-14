#ifndef __SHM_CHANNEL_H__
#define __SHM_CHANNEL_H__

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <climits>
#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <utility>

#include "basement/core/exception.h"
#include "basement/core/fd_guard.h"
#include "basement/core/logger/logger.h"
#include "basement/core/futex_compat.h"
#include "basement/transport/shm/shm_frame.h"

namespace star::transport {

static constexpr uint32_t kShmMaxSlots = 16;
static constexpr uint64_t kShmMagic = 0x535441524c455454ULL;
// v7：槽内 payload 前增加逐帧 schema 身份头（name/version/hash + encoding），
//     用于跨版本实时收发与诊断。
// v6：段名改用 FNV-1a（跨工具链稳定）。
// v5：header 增加持有者 pid 表，attach 时清扫孤儿段。
// v4：新增 per-slot 代际 gen[]，用于正确的 seqlock 复核。
// 旧版本的段名都带各自的 kShmVersion，升级后不会被命中（旧段成为不可达残留）。
static constexpr uint32_t kShmVersion = 7;
// 每个进程的每个通道占一个槽位；超限说明同一进程开了过多同通道实例
static constexpr uint32_t kShmMaxHolders = 32;
static constexpr uint32_t kShmCapacity = 4 * 1024 * 1024;
static constexpr uint32_t kShmSlotCount = 4;
static constexpr uint32_t kShmSlotSize = kShmCapacity / kShmSlotCount;
static constexpr int kInitRetryCount = 100;
static constexpr int kSpinRetryCount = 100;
// 写锁 owner-death 恢复：等待超过阈值且确认持锁进程已死，才强制复位
static constexpr int64_t kLockRecheckMs = 50;
static constexpr int64_t kOwnerDeathTimeoutMs = 200;


struct ShmHeader {
    uint64_t magic;
    uint32_t version;
    uint32_t header_bytes;
    std::atomic<uint64_t> seq;
    std::atomic<uint32_t> notify_seq;
    std::atomic<uint32_t> write_lock;
    // owner-death 恢复：记录持锁者，供等待者判断「持锁进程是否已崩溃」
    std::atomic<int32_t> write_lock_owner_pid;
    std::atomic<uint64_t> write_lock_owner_ns;
    std::atomic<uint32_t> lock_recovery;
    std::atomic<uint32_t> lock_recoveries;
    // 崩溃的写方会把 gen 留在奇数位；下一个持锁者需要把它归一化，
    // 否则后续写入的 +1 会让它变偶数，读方将把「已损坏的槽」当成稳定
    std::atomic<uint32_t> gen_parity_dirty;
    std::atomic<uint32_t> init_state;
    // 持有者 pid 表：进程异常退出（_exit/SIGKILL）时内核不会帮我们减引用计数，
    // 用 pid 存活性在 attach 时清扫孤儿段。0 = 空槽。
    std::atomic<uint32_t> holder_lock;
    std::atomic<int32_t> holder_lock_owner_pid;
    std::atomic<uint32_t> holder_recoveries;
    std::atomic<int32_t> holder_pids[kShmMaxHolders];
    uint32_t slot_size;
    uint32_t slot_count;
    uint32_t capacity;
    uint32_t reserved{0};
    std::atomic<uint32_t> sizes[kShmMaxSlots];
    std::atomic<uint64_t> write_ns[kShmMaxSlots];
    // per-slot seqlock 代际：写方在 memcpy 前置奇（写入中）、提交时置偶（稳定）。
    // 读方据此判断「本槽是否正在被覆写」，而不是靠全局 seq 推进量推断。
    std::atomic<uint32_t> gen[kShmMaxSlots];
};

// ── 写锁：owner 追踪与 owner-death 恢复 ──────────────────────────
inline auto lock_now_ns() -> uint64_t { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

inline auto process_alive(int32_t pid) -> bool {
    if (pid <= 0) {
        return false;
    }
    if (::kill(static_cast<pid_t>(pid), 0) == 0) {
        return true;
    }
    // EPERM：进程存在但无权限发信号；ESRCH：进程不存在
    return errno != ESRCH;
}

// 只有「确知 owner 已死」才复位；owner 未知（0）时保守等待，
// 避免把「刚拿到锁、尚未写入 owner」的正当持锁者误判为死亡。
inline void try_recover_dead_owner(ShmHeader* header) {
    const auto owner = header->write_lock_owner_pid.load(std::memory_order_relaxed);
    if (owner <= 0 || process_alive(owner)) {
        return;
    }
    // 复位操作本身需要串行化：两个等待者同时复位会互相踩掉对方刚拿到的锁
    if (header->lock_recovery.exchange(1, std::memory_order_acquire) != 0) {
        return;
    }
    if (header->write_lock.load(std::memory_order_relaxed) == 1 && header->write_lock_owner_pid.load(std::memory_order_relaxed) == owner && not process_alive(owner)) {
        logW << "ShmChannel: write_lock owner died, recovering: pid=" << owner;
        header->write_lock_owner_pid.store(0, std::memory_order_relaxed);
        header->gen_parity_dirty.store(1, std::memory_order_release);
        header->write_lock.store(0, std::memory_order_release);
        header->lock_recoveries.fetch_add(1, std::memory_order_relaxed);
        star::base::futex_wake(&header->write_lock, INT_MAX);
    }
    header->lock_recovery.store(0, std::memory_order_release);
}

// 必须在持锁状态下调用：把崩溃写方留下的奇数代际归一化为偶数
inline void fix_gen_parity_locked(ShmHeader* header) {
    if (header->gen_parity_dirty.exchange(0, std::memory_order_acq_rel) == 0) {
        return;
    }
    for (uint32_t i = 0; i < kShmMaxSlots; ++i) {
        const auto gen = header->gen[i].load(std::memory_order_relaxed);
        if ((gen & 1u) != 0u) {
            header->gen[i].store(gen + 1, std::memory_order_release);
        }
    }
    logW << "ShmChannel: normalized slot generations after owner death";
}

inline void spin_lock(ShmHeader* header) {
    auto& lock = header->write_lock;
    for (int i = 0; i < kSpinRetryCount; ++i) {
        if (lock.exchange(1, std::memory_order_acquire) == 0) {
            header->write_lock_owner_pid.store(static_cast<int32_t>(::getpid()), std::memory_order_relaxed);
            header->write_lock_owner_ns.store(lock_now_ns(), std::memory_order_relaxed);
            fix_gen_parity_locked(header);
            return;
        }
    }
    auto waited_ms = int64_t{0};
    while (true) {
        if (lock.load(std::memory_order_relaxed) == 0) {
            if (lock.exchange(1, std::memory_order_acquire) == 0) {
                header->write_lock_owner_pid.store(static_cast<int32_t>(::getpid()), std::memory_order_relaxed);
                header->write_lock_owner_ns.store(lock_now_ns(), std::memory_order_relaxed);
                fix_gen_parity_locked(header);
                return;
            }
        }
        // 有界等待，周期性复核 owner 是否已死（无限等待会让崩溃变成永久停摆）
        star::base::futex_wait(&lock, 1, kLockRecheckMs);
        waited_ms += kLockRecheckMs;
        if (waited_ms >= kOwnerDeathTimeoutMs) {
            try_recover_dead_owner(header);
            waited_ms = 0;
        }
    }
}

inline void spin_unlock(ShmHeader* header) {
    header->write_lock_owner_pid.store(0, std::memory_order_relaxed);
    header->write_lock.store(0, std::memory_order_release);
    star::base::futex_wake(&header->write_lock, 1);
}

// ── 持有者表：孤儿段检测与回收 ──────────────────────────────────
inline auto process_is_alive(int32_t pid) -> bool {
    if (pid <= 0) {
        return false;
    }
    if (::kill(pid, 0) == 0) {
        return true;
    }
    // EPERM 说明进程存在但无权限（仍算活着）；只有 ESRCH 才是确定已死
    return errno != ESRCH;
}

inline void holder_lock(ShmHeader* header) {
    if (header->holder_lock.exchange(1, std::memory_order_acquire) == 0) {
        header->holder_lock_owner_pid.store(static_cast<int32_t>(::getpid()), std::memory_order_relaxed);
        return;
    }
    const auto begin = std::chrono::steady_clock::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (header->holder_lock.load(std::memory_order_relaxed) == 0) {
            if (header->holder_lock.exchange(1, std::memory_order_acquire) == 0) {
                header->holder_lock_owner_pid.store(static_cast<int32_t>(::getpid()), std::memory_order_relaxed);
                return;
            }
            continue;
        }
        // 临界区里没有阻塞调用；持锁进程若已死或持锁过久，强制复位，避免永久卡住
        const auto owner = header->holder_lock_owner_pid.load(std::memory_order_relaxed);
        const auto waited_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
        if ((owner != 0 && not process_is_alive(owner)) || waited_ms > 5000) {
            header->holder_lock.store(0, std::memory_order_release);
        }
    }
}

inline void holder_unlock(ShmHeader* header) {
    header->holder_lock_owner_pid.store(0, std::memory_order_relaxed);
    header->holder_lock.store(0, std::memory_order_release);
    star::base::futex_wake(&header->holder_lock, 1);
}

// 返回清扫后仍存活的持有者数量；已死进程的槽位顺手清掉
inline auto sweep_dead_holders(ShmHeader* header) -> int32_t {
    int32_t live = 0;
    for (uint32_t i = 0; i < kShmMaxHolders; ++i) {
        const auto pid = header->holder_pids[i].load(std::memory_order_acquire);
        if (pid == 0) {
            continue;
        }
        if (process_is_alive(pid)) {
            ++live;
        } else {
            header->holder_pids[i].store(0, std::memory_order_release);
        }
    }
    return live;
}

inline auto add_holder(ShmHeader* header) -> bool {
    const auto self = static_cast<int32_t>(::getpid());
    for (uint32_t i = 0; i < kShmMaxHolders; ++i) {
        if (header->holder_pids[i].load(std::memory_order_acquire) == 0) {
            header->holder_pids[i].store(self, std::memory_order_release);
            return true;
        }
    }
    return false;
}

// 移除自己的一个槽位并清扫死进程；返回剩余存活持有者数量
inline auto remove_holder(ShmHeader* header) -> int32_t {
    const auto self = static_cast<int32_t>(::getpid());
    for (uint32_t i = 0; i < kShmMaxHolders; ++i) {
        if (header->holder_pids[i].load(std::memory_order_acquire) == self) {
            header->holder_pids[i].store(0, std::memory_order_release);
            break;
        }
    }
    return sweep_dead_holders(header);
}

inline void initialize_header(ShmHeader* header, uint32_t slot_size, uint32_t slot_count, uint32_t capacity) {
    // 调用方必须持有 holder_lock；init_state 最后置 1，attach 侧据此等待
    header->init_state.store(0, std::memory_order_relaxed);
    header->write_lock.store(0, std::memory_order_relaxed);
    header->write_lock_owner_pid.store(0, std::memory_order_relaxed);
    header->write_lock_owner_ns.store(0, std::memory_order_relaxed);
    header->lock_recovery.store(0, std::memory_order_relaxed);
    header->lock_recoveries.store(0, std::memory_order_relaxed);
    header->gen_parity_dirty.store(0, std::memory_order_relaxed);
    header->holder_lock.store(0, std::memory_order_relaxed);
    header->holder_lock_owner_pid.store(0, std::memory_order_relaxed);
    header->holder_recoveries.store(0, std::memory_order_relaxed);
    for (uint32_t i = 0; i < kShmMaxHolders; ++i) {
        header->holder_pids[i].store(0, std::memory_order_relaxed);
    }
    header->magic = kShmMagic;
    header->version = kShmVersion;
    header->header_bytes = sizeof(ShmHeader);
    header->seq.store(0, std::memory_order_relaxed);
    header->notify_seq.store(0, std::memory_order_relaxed);
    header->slot_size = slot_size;
    header->slot_count = slot_count;
    header->capacity = capacity;
    for (uint32_t i = 0; i < kShmMaxSlots; ++i) {
        header->sizes[i].store(0, std::memory_order_relaxed);
        header->write_ns[i].store(0, std::memory_order_relaxed);
        header->gen[i].store(0, std::memory_order_relaxed);
    }
}

class ShmChannel {
  public:
    ShmChannel(std::string name, uint32_t capacity) : name_(std::move(name)), capacity_(capacity) {
        if (capacity_ == 0) {
            throw star::excep::RuntimeError("ShmChannel capacity is zero");
        }

        const auto slot_count = resolve_slot_count();
        const auto slot_size = resolve_slot_size(capacity_, slot_count);
        const auto cap = slot_size * slot_count;
        const auto map_size = static_cast<size_t>(sizeof(ShmHeader)) + static_cast<size_t>(cap);

        bool created = false;
        int fd = ::shm_open(name_.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
        if (fd >= 0) {
            created = true;
        } else {
            fd = ::shm_open(name_.c_str(), O_CREAT | O_RDWR, 0666);
            if (fd < 0) {
                throw star::excep::RuntimeError("shm_open failed: ", name_, ", errno=", errno);
            }
        }
        fd_guard_ = star::base::FdGuard(fd);

        if (created) {
            if (::ftruncate(fd_guard_.get(), static_cast<off_t>(map_size)) != 0) {
                throw star::excep::RuntimeError("ftruncate failed: ", name_, ", errno=", errno);
            }
        } else {
            struct stat st{};
            if (::fstat(fd_guard_.get(), &st) != 0) {
                throw star::excep::RuntimeError("fstat failed: ", name_, ", errno=", errno);
            }
            if (static_cast<size_t>(st.st_size) < sizeof(ShmHeader)) {
                throw star::excep::RuntimeError("ShmChannel existing shm too small: ", name_);
            }
        }

        void* p = ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_guard_.get(), 0);
        if (p == MAP_FAILED) {
            throw star::excep::RuntimeError("mmap failed: ", name_, ", errno=", errno);
        }

        mapping_.reset(p, [map_size](void* addr) {
            if (addr != nullptr && addr != MAP_FAILED) {
                ::munmap(addr, map_size);
            }
        });

        header_ = static_cast<ShmHeader*>(mapping_.get());

        if (created) {
            // 先把 header 与持有者表写完整，再置 init_state=1 放行 attach 方；
            // 即使本进程在初始化中途崩溃，留下的也是可被清扫的结构
            holder_lock(header_);
            initialize_header(header_, slot_size, slot_count, cap);
            if (not add_holder(header_)) {
                holder_unlock(header_);
                throw star::excep::RuntimeError("ShmChannel failed, too many holders: ", name_);
            }
            header_->init_state.store(1, std::memory_order_release);
            holder_unlock(header_);
        } else {
            bool ready = false;
            for (int i = 0; i < kInitRetryCount; ++i) {
                if (header_->init_state.load(std::memory_order_acquire) == 1) {
                    ready = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            holder_lock(header_);
            auto recovered = false;
            if (not ready) {
                // 创建者在初始化中途崩溃：只有确认没有存活持有者才重建
                if (sweep_dead_holders(header_) == 0) {
                    initialize_header(header_, slot_size, slot_count, cap);
                    recovered = true;
                } else {
                    holder_unlock(header_);
                    throw star::excep::RuntimeError("ShmChannel init timeout: ", name_);
                }
            } else {
                const auto live = sweep_dead_holders(header_);
                if (live == 0) {
                    // 孤儿段：所有持有者进程都已退出（_exit/SIGKILL 跳过了析构）
                    initialize_header(header_, slot_size, slot_count, cap);
                    recovered = true;
                } else {
                    // attach 前校验段布局：旧版本/损坏段若继续使用，会按错误布局解析内存
                    if (header_->magic != kShmMagic) {
                        holder_unlock(header_);
                        throw star::excep::RuntimeError("ShmChannel attach failed, bad magic: ", name_);
                    }
                    if (header_->version != kShmVersion) {
                        holder_unlock(header_);
                        throw star::excep::RuntimeError("ShmChannel attach failed, version mismatch: ", name_, ", segment=", header_->version, ", expected=", kShmVersion);
                    }
                    if (header_->header_bytes != sizeof(ShmHeader)) {
                        holder_unlock(header_);
                        throw star::excep::RuntimeError("ShmChannel attach failed, header size mismatch: ", name_, ", segment=", header_->header_bytes, ", expected=", sizeof(ShmHeader));
                    }
                    if (header_->slot_count == 0 || header_->slot_count > kShmMaxSlots || header_->slot_size == 0) {
                        holder_unlock(header_);
                        throw star::excep::RuntimeError("ShmChannel attach failed, invalid layout: ", name_);
                    }
                }
            }
            if (not add_holder(header_)) {
                holder_unlock(header_);
                throw star::excep::RuntimeError("ShmChannel failed, too many holders: ", name_);
            }
            if (recovered) {
                header_->holder_recoveries.fetch_add(1, std::memory_order_relaxed);
            }
            header_->init_state.store(1, std::memory_order_release);
            holder_unlock(header_);
        }

        slot_size_ = header_->slot_size;
        slot_count_ = header_->slot_count;
        capacity_ = header_->capacity;
    }

    ~ShmChannel() {
        if (header_ == nullptr) {
            return;
        }
        holder_lock(header_);
        const auto live = remove_holder(header_);
        if (live == 0) {
            ::shm_unlink(name_.c_str());
        }
        holder_unlock(header_);
    }

    auto capacity() const -> uint32_t { return capacity_; }
    auto slot_size() const -> uint32_t { return slot_size_; }
    auto slot_count() const -> uint32_t { return slot_count_; }

    // 观测：本通道发生过多少次写锁 owner-death 恢复
    auto lock_recovery_count() const -> uint32_t { return header_ != nullptr ? header_->lock_recoveries.load(std::memory_order_relaxed) : 0; }

    // 观测：该段被多少次 attach 判定为孤儿并重建（持有者进程全部已死）
    auto orphan_recovery_count() const -> uint32_t { return header_ != nullptr ? header_->holder_recoveries.load(std::memory_order_relaxed) : 0; }

    struct Loan {
        ShmHeader* header{nullptr};
        uint8_t* slot_ptr{nullptr};
        uint32_t slot{0};
        uint32_t capacity{0};
        uint64_t seq{0};

        Loan() = default;
        // 不可拷贝：拷贝会导致两个析构各自解锁一次（双重解锁）
        Loan(const Loan&) = delete;
        auto operator=(const Loan&) -> Loan& = delete;
        Loan(Loan&& other) noexcept
            : header(std::exchange(other.header, nullptr)), slot_ptr(other.slot_ptr), slot(other.slot), capacity(other.capacity), seq(other.seq) {}
        auto operator=(Loan&& other) noexcept -> Loan& {
            if (this != &other) {
                release();
                header = std::exchange(other.header, nullptr);
                slot_ptr = other.slot_ptr;
                slot = other.slot;
                capacity = other.capacity;
                seq = other.seq;
            }
            return *this;
        }
        ~Loan() { release(); }

        auto data() const -> uint8_t* { return slot_ptr; }

        void commit(uint32_t size) {
            if (header == nullptr) {
                return;
            }
            header->sizes[slot].store(size, std::memory_order_relaxed);
            // 先让本槽代际变偶（稳定），再推进全局 seq：
            // 读方看到 seq=N 时必然也能看到 gen 为偶、数据完整
            header->gen[slot].fetch_add(1, std::memory_order_release);
            header->seq.store(seq, std::memory_order_release);
            header->notify_seq.fetch_add(1, std::memory_order_release);
            ShmChannel::wake_waiters(header);
            spin_unlock(header);
            header = nullptr;
        }

      private:
        void release() {
            if (header != nullptr) {
                // 未提交就释放（异常/提前返回）：把代际恢复为偶，
                // 否则读方会永久把本槽判为「写入中」而停止收消息
                header->gen[slot].fetch_add(1, std::memory_order_release);
                spin_unlock(header);
                header = nullptr;
            }
        }
    };

    auto loan(uint32_t size) -> Loan {
        if (slot_count_ == 0 || slot_count_ > kShmMaxSlots || slot_size_ == 0) {
            throw star::excep::RuntimeError("ShmChannel loan failed, invalid header layout");
        }
        if (size > slot_size_) {
            throw star::excep::RuntimeError("ShmChannel loan failed, size too large: ", size);
        }
        spin_lock(header_);
        const auto next_seq = header_->seq.load(std::memory_order_relaxed) + 1;
        const auto idx = static_cast<uint32_t>(next_seq % slot_count_);
        // 进入写入窗口：代际置奇，读方据此拒绝本槽（含 memcpy 进行中的整个区间）
        header_->gen[idx].fetch_add(1, std::memory_order_release);
        auto* slot = static_cast<uint8_t*>(mapping_.get()) + sizeof(ShmHeader) + static_cast<size_t>(idx) * slot_size_;
        Loan out;
        out.header = header_;
        out.slot_ptr = slot;
        out.slot = idx;
        out.capacity = slot_size_;
        out.seq = next_seq;
        return out;
    }

    // 写入一帧：槽内布局 = 帧头 + schema 身份 + payload。身份逐帧携带，
    // 因为滚动升级时同一通道上可能同时存在不同版本的生产者。
    void write_framed(const star::schema::SchemaTypeId& schema_id, uint8_t encoding, const uint8_t* payload, uint32_t size) {
        if (schema_id.name.size() > kShmFrameMaxNameLen) {
            throw star::excep::RuntimeError("ShmChannel write_framed failed, schema name too long: ", schema_id.name.size());
        }
        const auto header_size = sizeof(ShmFrameHeader) + schema_id.name.size();
        const auto total = header_size + static_cast<size_t>(size);
        if (total > slot_size_) {
            throw star::excep::RuntimeError("ShmChannel write_framed failed, frame exceeds slot: ", total, " > ", slot_size_);
        }
        auto loaned = loan(static_cast<uint32_t>(total));
        auto header = ShmFrameHeader{};
        header.magic = kShmFrameMagic;
        header.encoding = encoding;
        header.flags = 0;
        header.name_len = static_cast<uint16_t>(schema_id.name.size());
        header.schema_version = schema_id.version;
        header.schema_hash = schema_id.hash;
        header.payload_size = size;
        std::memcpy(loaned.slot_ptr, &header, sizeof(header));
        if (not schema_id.name.empty()) {
            std::memcpy(loaned.slot_ptr + sizeof(header), schema_id.name.data(), schema_id.name.size());
        }
        if (size > 0 and payload != nullptr) {
            std::memcpy(loaned.slot_ptr + header_size, payload, size);
        }
        header_->write_ns[loaned.slot].store(now_steady_ns(), std::memory_order_relaxed);
        loaned.commit(static_cast<uint32_t>(total));
    }

    void write(const void* data, uint32_t size) {
        if (slot_count_ == 0 || slot_count_ > kShmMaxSlots || slot_size_ == 0) {
            throw star::excep::RuntimeError("ShmChannel write failed, invalid header layout");
        }
        if (size > slot_size_) {
            throw star::excep::RuntimeError("ShmChannel write failed, size too large: ", size);
        }
        auto loaned = loan(size);
        if (data != nullptr && size > 0) {
            std::memcpy(loaned.slot_ptr, data, size);
        }
        header_->write_ns[loaned.slot].store(now_steady_ns(), std::memory_order_relaxed);
        loaned.commit(size);
    }

    struct ReadResult {
        uint32_t size{0};
        uint64_t seq{0};
        uint32_t slot{0};
        uint64_t dropped{0};
        uint64_t write_ns{0};
        uint32_t gen{0};
    };

    auto read(uint64_t* last_seq) const -> std::optional<ReadResult> {
        if (last_seq == nullptr) {
            return std::nullopt;
        }
        const auto seq = header_->seq.load(std::memory_order_acquire);
        if (seq == *last_seq) {
            return std::nullopt;
        }
        if (slot_count_ == 0 || slot_count_ > kShmMaxSlots || slot_size_ == 0) {
            *last_seq = seq;
            return std::nullopt;
        }
        const auto slot = static_cast<uint32_t>(seq % slot_count_);
        // seqlock 前置检查：写方正在覆盖本槽时不推进 last_seq，等下次再读
        const auto gen1 = header_->gen[slot].load(std::memory_order_acquire);
        if ((gen1 & 1u) != 0u) {
            return std::nullopt;
        }
        const auto size = header_->sizes[slot].load(std::memory_order_relaxed);
        if (size > slot_size_) {
            *last_seq = seq;
            return std::nullopt;
        }
        // 后置检查：读取 size 期间若发生过一次完整覆写，本帧作废
        const auto gen2 = header_->gen[slot].load(std::memory_order_acquire);
        if (gen2 != gen1) {
            return std::nullopt;
        }
        const auto dropped = seq > (*last_seq + 1) ? (seq - *last_seq - 1) : 0;
        *last_seq = seq;
        ReadResult out;
        out.size = size;
        out.seq = seq;
        out.slot = slot;
        out.dropped = dropped;
        out.write_ns = header_->write_ns[slot].load(std::memory_order_relaxed);
        out.gen = gen1;
        return out;
    }

    // 供读侧构造 ShmSlotView：视图必须持有本槽代际地址，才能在解码前后复核
    auto gen_addr(uint32_t slot) const -> const std::atomic<uint32_t>* {
        if (header_ == nullptr || slot >= kShmMaxSlots) {
            return nullptr;
        }
        return &header_->gen[slot];
    }

    auto mapping() const -> std::shared_ptr<void> { return mapping_; }

    auto current_seq() const -> uint64_t {
        if (header_ == nullptr) {
            return 0;
        }
        return header_->seq.load(std::memory_order_acquire);
    }

    void wait_for_update(uint64_t observed_seq, int64_t timeout_ms) const {
        if (header_ == nullptr) {
            return;
        }
        if (current_seq() != observed_seq) {
            return;
        }
        const auto expected = header_->notify_seq.load(std::memory_order_acquire);
        if (current_seq() != observed_seq) {
            return;
        }
        futex_wait(&header_->notify_seq, expected, timeout_ms);
    }

    void wake_waiters() const {
        if (header_ == nullptr) {
            return;
        }
        header_->notify_seq.fetch_add(1, std::memory_order_release);
        wake_waiters(header_);
    }

    auto payload_ptr(uint32_t slot) const -> const uint8_t* {
        if (slot_count_ == 0 || slot >= slot_count_) {
            return nullptr;
        }
        return static_cast<const uint8_t*>(mapping_.get()) + sizeof(ShmHeader) + static_cast<size_t>(slot) * slot_size_;
    }

    // 供读侧做 seqlock 复核：槽位会被 seq % slot_count 覆写，视图必须带代际信息

    static auto now_steady_ns() -> uint64_t { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

  private:
    static int futex_wait(std::atomic<uint32_t>* addr, uint32_t expected, int64_t timeout_ms) {
        return star::base::futex_wait(addr, expected, timeout_ms);
    }

    static void wake_waiters(ShmHeader* header) {
        if (header == nullptr) {
            return;
        }
        star::base::futex_wake(&header->notify_seq, INT_MAX);
    }

    static auto resolve_slot_count() -> uint32_t { return kShmSlotCount; }

    static auto resolve_capacity(const std::string& channel_name) -> uint32_t {
        (void)channel_name;
        return kShmCapacity;
    }

    static auto resolve_slot_size(uint32_t /*capacity*/, uint32_t /*slots*/) -> uint32_t { return kShmSlotSize; }

    std::string name_;
    uint32_t capacity_{0};
    uint32_t slot_size_{0};
    uint32_t slot_count_{0};
    star::base::FdGuard fd_guard_;
    std::shared_ptr<void> mapping_;
    ShmHeader* header_{nullptr};
};

}  // namespace star::transport

#endif  // __SHM_CHANNEL_H__
