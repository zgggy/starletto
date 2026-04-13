#ifndef __SHM_CHANNEL_H__
#define __SHM_CHANNEL_H__

#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <climits>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>

#include "basement/core/exception.h"

namespace star::transport {

static constexpr uint32_t kShmMaxSlots = 16;
static constexpr uint64_t kShmMagic = 0x535441524c455454ULL;
static constexpr uint32_t kShmVersion = 3;
static constexpr uint32_t kShmCapacity = 4 * 1024 * 1024;
static constexpr uint32_t kShmSlotCount = 4;
static constexpr uint32_t kShmSlotSize = kShmCapacity / kShmSlotCount;

struct ShmHeader {
    uint64_t magic;
    uint32_t version;
    uint32_t header_bytes;
    std::atomic<uint64_t> seq;
    std::atomic<uint32_t> notify_seq;
    uint32_t slot_size;
    uint32_t slot_count;
    uint32_t capacity;
    uint32_t reserved{0};
    std::atomic<uint32_t> sizes[kShmMaxSlots];
    std::atomic<uint64_t> publish_ns[kShmMaxSlots];
};

class ShmChannel {
  public:
    ShmChannel(std::string name, uint32_t capacity) : name_(std::move(name)), capacity_(capacity) {
        if (capacity_ == 0) {
            throw star::excep::RuntimeError("ShmChannel capacity is zero");
        }

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
        fd_ = fd;

        const auto slot_count = resolve_slot_count();
        const auto slot_size = resolve_slot_size(capacity_, slot_count);
        const auto cap = slot_size * slot_count;
        const auto map_size = static_cast<size_t>(sizeof(ShmHeader)) + static_cast<size_t>(cap);
        if (::ftruncate(fd_, static_cast<off_t>(map_size)) != 0) {
            const auto e = errno;
            ::close(fd_);
            fd_ = -1;
            throw star::excep::RuntimeError("ftruncate failed: ", name_, ", errno=", e);
        }

        void* p = ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (p == MAP_FAILED) {
            const auto e = errno;
            ::close(fd_);
            fd_ = -1;
            throw star::excep::RuntimeError("mmap failed: ", name_, ", errno=", e);
        }

        mapping_.reset(p, [map_size](void* addr) {
            if (addr != nullptr && addr != MAP_FAILED) {
                ::munmap(addr, map_size);
            }
        });

        header_ = static_cast<ShmHeader*>(mapping_.get());
        const auto needs_init = created || header_->magic != kShmMagic || header_->version != kShmVersion || header_->header_bytes != sizeof(ShmHeader);
        if (needs_init) {
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
                header_->publish_ns[i].store(0, std::memory_order_relaxed);
            }
        }
        if (header_->slot_count == 0 || header_->slot_count > kShmMaxSlots) {
            header_->slot_count = slot_count;
        }
        if (header_->slot_size == 0) {
            header_->slot_size = slot_size;
        }
        if (header_->capacity == 0) {
            header_->capacity = cap;
        }
        slot_size_ = header_->slot_size;
        slot_count_ = header_->slot_count;
        capacity_ = header_->capacity;
    }

    ~ShmChannel() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    auto capacity() const -> uint32_t { return capacity_; }
    auto slot_size() const -> uint32_t { return slot_size_; }
    auto slot_count() const -> uint32_t { return slot_count_; }

    struct Loan {
        std::unique_lock<std::mutex> lock;
        ShmHeader* header{nullptr};
        uint8_t* slot_ptr{nullptr};
        uint32_t slot{0};
        uint32_t capacity{0};
        uint64_t seq{0};

        auto data() const -> uint8_t* { return slot_ptr; }

        void commit(uint32_t size) {
            if (header == nullptr) {
                return;
            }
            header->sizes[slot].store(size, std::memory_order_relaxed);
            header->seq.store(seq, std::memory_order_release);
            header->notify_seq.fetch_add(1, std::memory_order_release);
            ShmChannel::wake_waiters(header);
            header = nullptr;
        }
    };

    auto loan(uint32_t size) -> Loan {
        if (slot_count_ == 0 || slot_count_ > kShmMaxSlots || slot_size_ == 0) {
            throw star::excep::RuntimeError("ShmChannel loan failed, invalid header layout");
        }
        if (size > slot_size_) {
            throw star::excep::RuntimeError("ShmChannel loan failed, size too large: ", size);
        }
        std::unique_lock<std::mutex> lock(write_mutex_);
        const auto next_seq = header_->seq.load(std::memory_order_relaxed) + 1;
        const auto idx = static_cast<uint32_t>(next_seq % slot_count_);
        auto* slot = static_cast<uint8_t*>(mapping_.get()) + sizeof(ShmHeader) + static_cast<size_t>(idx) * slot_size_;
        Loan out;
        out.lock = std::move(lock);
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
        header_->publish_ns[loaned.slot].store(now_steady_ns(), std::memory_order_relaxed);
        loaned.commit(size);
    }

    struct ReadResult {
        uint32_t size{0};
        uint64_t seq{0};
        uint32_t slot{0};
        uint64_t dropped{0};
        uint64_t publish_ns{0};
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
        const auto size = header_->sizes[slot].load(std::memory_order_relaxed);
        if (size > slot_size_) {
            *last_seq = seq;
            return std::nullopt;
        }
        const auto dropped = seq > (*last_seq + 1) ? (seq - *last_seq - 1) : 0;
        *last_seq = seq;
        ReadResult out;
        out.size = size;
        out.seq = seq;
        out.slot = slot;
        out.dropped = dropped;
        out.publish_ns = header_->publish_ns[slot].load(std::memory_order_relaxed);
        return out;
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

    static auto now_steady_ns() -> uint64_t { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

  private:
    static int futex_wait(std::atomic<uint32_t>* addr, uint32_t expected, int64_t timeout_ms) {
        auto ts = ::timespec{};
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = static_cast<long>((timeout_ms % 1000) * 1000000);
        return static_cast<int>(::syscall(SYS_futex, reinterpret_cast<uint32_t*>(addr), FUTEX_WAIT, expected, &ts, nullptr, 0));
    }

    static void wake_waiters(ShmHeader* header) {
        if (header == nullptr) {
            return;
        }
        ::syscall(SYS_futex, reinterpret_cast<uint32_t*>(&header->notify_seq), FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
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
    int fd_{-1};
    std::shared_ptr<void> mapping_;
    ShmHeader* header_{nullptr};
    mutable std::mutex write_mutex_;
};

}  // namespace star::transport

#endif  // __SHM_CHANNEL_H__
