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

namespace star::transport {

static constexpr uint32_t kShmMaxSlots = 16;
static constexpr uint64_t kShmMagic = 0x535441524c455454ULL;
// v4：新增 per-slot 代际 gen[]，用于正确的 seqlock 复核（v3 的全局 seq 判定
// 无法覆盖「写方正在 memcpy、seq 尚未推进」的窗口）
static constexpr uint32_t kShmVersion = 4;
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
    std::atomic<uint32_t> ref_count;
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
            header_->init_state.store(0, std::memory_order_relaxed);
            header_->write_lock.store(0, std::memory_order_relaxed);
            header_->write_lock_owner_pid.store(0, std::memory_order_relaxed);
            header_->write_lock_owner_ns.store(0, std::memory_order_relaxed);
            header_->lock_recovery.store(0, std::memory_order_relaxed);
            header_->lock_recoveries.store(0, std::memory_order_relaxed);
            header_->gen_parity_dirty.store(0, std::memory_order_relaxed);
            header_->ref_count.store(0, std::memory_order_relaxed);
            header_->magic = kShmMagic;
            header_->version = kShmVersion;
            header_->header_bytes = sizeof(ShmHeader);
            header_->seq.store(0, std::memory_order_relaxed);
            header_->notify_seq.store(0, std::memory_order_relaxed);
            header_->slot_size = slot_size;
            header_->slot_count = slot_count;
            header_->capacity = cap;
            for (uint32_t i = 0; i < kShmMaxSlots; ++i) {
                header_->sizes[i].store(0, std::memory_order_relaxed);
                header_->write_ns[i].store(0, std::memory_order_relaxed);
                header_->gen[i].store(0, std::memory_order_relaxed);
            }
            header_->ref_count.fetch_add(1, std::memory_order_acq_rel);
            header_->init_state.store(1, std::memory_order_release);
        } else {
            for (int i = 0; i < kInitRetryCount; ++i) {
                if (header_->init_state.load(std::memory_order_acquire) == 1) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (header_->init_state.load(std::memory_order_acquire) != 1) {
                throw star::excep::RuntimeError("ShmChannel init timeout: ", name_);
            }
            // attach 前校验段布局：旧版本/损坏段若继续使用，会按错误布局解析内存
            // （越界或静默读错）。校验失败时不得回写别人的 header。
            if (header_->magic != kShmMagic) {
                throw star::excep::RuntimeError("ShmChannel attach failed, bad magic: ", name_);
            }
            if (header_->version != kShmVersion) {
                throw star::excep::RuntimeError("ShmChannel attach failed, version mismatch: ", name_, ", segment=", header_->version, ", expected=", kShmVersion,
                                                "（旧版本段残留，请清理该段或更换段名）");
            }
            if (header_->header_bytes != sizeof(ShmHeader)) {
                throw star::excep::RuntimeError("ShmChannel attach failed, header size mismatch: ", name_, ", segment=", header_->header_bytes, ", expected=", sizeof(ShmHeader));
            }
            if (header_->slot_count == 0 || header_->slot_count > kShmMaxSlots || header_->slot_size == 0) {
                throw star::excep::RuntimeError("ShmChannel attach failed, invalid layout: ", name_);
            }
            header_->ref_count.fetch_add(1, std::memory_order_acq_rel);
        }

        slot_size_ = header_->slot_size;
        slot_count_ = header_->slot_count;
        capacity_ = header_->capacity;
    }

    ~ShmChannel() {
        if (header_ != nullptr) {
            const auto remaining = header_->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            if (remaining == 1) {
                ::shm_unlink(name_.c_str());
            }
        }
    }

    auto capacity() const -> uint32_t { return capacity_; }
    auto slot_size() const -> uint32_t { return slot_size_; }
    auto slot_count() const -> uint32_t { return slot_count_; }

    // 观测：本通道发生过多少次写锁 owner-death 恢复
    auto lock_recovery_count() const -> uint32_t { return header_ != nullptr ? header_->lock_recoveries.load(std::memory_order_relaxed) : 0; }

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
