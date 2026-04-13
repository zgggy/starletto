#ifndef __NATIVE_WRITER_H__
#define __NATIVE_WRITER_H__

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "basement/transport/api/message_bus.h"

class NativeWriter final : public star::transport::Writer {
  public:
    NativeWriter(std::shared_ptr<NativeMessageBus> bus, std::string channel_name) : bus_(std::move(bus)), channel_name_(std::move(channel_name)) {}

    void write(const star::transport::MessageView& message) override {
        auto bus = bus_.lock();
        if (not bus) {
            return;
        }
        bus->publish_internal(channel_name_, message);
    }

  private:
    std::weak_ptr<NativeMessageBus> bus_;
    std::string channel_name_;
};

class InprocAckWriter final : public star::transport::AckWriter {
  public:
    InprocAckWriter(std::shared_ptr<NativeMessageBus> bus, std::string channel_name, int64_t ack_timeout_ms)
        : bus_(std::move(bus)), channel_name_(std::move(channel_name)), ack_timeout_ms_(ack_timeout_ms) {}

    void write(const star::transport::MessageView& message) override {
        auto bus = bus_.lock();
        if (not bus) {
            return;
        }
        bus->publish_internal(channel_name_, message);
    }

    auto write_with_ack(const star::transport::MessageView& message, int64_t timeout_ms) -> std::future<star::transport::AckResult> override {
        auto promise = std::make_shared<std::promise<star::transport::AckResult>>();
        auto future = promise->get_future();
        auto timeout = timeout_ms > 0 ? timeout_ms : ack_timeout_ms_;

        std::thread([this, promise, message, timeout]() {
            try {
                auto begin = std::chrono::steady_clock::now();
                auto bus = bus_.lock();
                if (not bus) {
                    promise->set_value({false, "bus expired", 0});
                    return;
                }
                bus->publish_internal(channel_name_, message);

                if (timeout > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                    promise->set_value({true, "ok (fire-and-forget)", 0});
                } else {
                    const auto latency_us = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count());
                    promise->set_value({true, "ok", latency_us});
                }
            } catch (const std::exception& e) {
                promise->set_value({false, e.what(), 0});
            } catch (...) {
                promise->set_value({false, "unknown error", 0});
            }
        }).detach();

        return future;
    }

    void set_timeout_callback(star::transport::AckWriter::TimeoutCallback cb) override { timeout_callback_ = std::move(cb); }

  private:
    std::weak_ptr<NativeMessageBus> bus_;
    std::string channel_name_;
    int64_t ack_timeout_ms_{0};
    star::transport::AckWriter::TimeoutCallback timeout_callback_;
};

#endif  // __NATIVE_WRITER_H__
