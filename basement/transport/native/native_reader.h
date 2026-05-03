#ifndef __NATIVE_READER_H__
#define __NATIVE_READER_H__

#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "basement/transport/api/message_bus.h"

class NativeMessageBus;

class NativeReader final : public star::transport::Reader {
  public:
    NativeReader(std::shared_ptr<NativeMessageBus> bus, std::string channel_name, uint64_t id)
        : bus_(std::move(bus)), channel_name_(std::move(channel_name)), id_(id) {}

    ~NativeReader() override {
        auto bus = bus_.lock();
        if (not bus) {
            return;
        }
        bus->erase_reader(channel_name_, id_);
    }

    auto peek_latest() const -> std::optional<star::transport::MessageView> override {
        auto bus = bus_.lock();
        if (not bus) {
            return std::nullopt;
        }
        return bus->peek_latest(channel_name_);
    }

    auto has_data() const -> bool override {
        return peek_latest().has_value();
    }

  private:
    std::weak_ptr<NativeMessageBus> bus_;
    std::string channel_name_;
    uint64_t id_{0};
};

#endif  // __NATIVE_READER_H__
