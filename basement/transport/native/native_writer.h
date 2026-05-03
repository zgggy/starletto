#ifndef __NATIVE_WRITER_H__
#define __NATIVE_WRITER_H__

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "basement/transport/api/message_bus.h"

class NativeWriter final : public star::transport::Writer {
  public:
    NativeWriter(std::shared_ptr<NativeMessageBus> bus, std::string channel_name) : bus_(std::move(bus)), channel_name_(std::move(channel_name)) {}

    auto write(const star::transport::MessageView& message) -> star::transport::WriteResult override {
        auto bus = bus_.lock();
        if (not bus) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::BUS_EXPIRED;
            return out;
        }
        try {
            bus->write_internal(channel_name_, message, nullptr);
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::OK;
            out.inproc_ok = true;
            return out;
        } catch (const std::exception& e) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            out.error_message = e.what();
            return out;
        } catch (...) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            return out;
        }
    }

    auto write_serialized(const star::transport::SerializedMessage& message) -> star::transport::WriteResult override {
        auto bus = bus_.lock();
        if (not bus) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::BUS_EXPIRED;
            return out;
        }
        try {
            bus->write_internal(channel_name_, message.view, &message.serializer);
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::OK;
            out.inproc_ok = true;
            return out;
        } catch (const std::exception& e) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            out.error_message = e.what();
            return out;
        } catch (...) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            return out;
        }
    }

  private:
    std::weak_ptr<NativeMessageBus> bus_;
    std::string channel_name_;
};

class InprocSyncWriter final : public star::transport::DeliveryWriter {
  public:
    InprocSyncWriter(std::shared_ptr<NativeMessageBus> bus, std::string channel_name) : bus_(std::move(bus)), channel_name_(std::move(channel_name)) {}

    ~InprocSyncWriter() {
        std::vector<std::future<void>> pending;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending = std::move(pending_acks_);
        }
        for (auto& f : pending) {
            if (f.valid()) {
                f.wait();
            }
        }
    }

    auto write(const star::transport::MessageView& message) -> star::transport::WriteResult override {
        auto bus = bus_.lock();
        if (not bus) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::BUS_EXPIRED;
            return out;
        }
        try {
            bus->write_internal(channel_name_, message, nullptr);
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::OK;
            out.inproc_ok = true;
            return out;
        } catch (const std::exception& e) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            out.error_message = e.what();
            return out;
        } catch (...) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            return out;
        }
    }

    auto write_serialized(const star::transport::SerializedMessage& message) -> star::transport::WriteResult override {
        auto bus = bus_.lock();
        if (not bus) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::BUS_EXPIRED;
            return out;
        }
        try {
            bus->write_internal(channel_name_, message.view, &message.serializer);
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::OK;
            out.inproc_ok = true;
            return out;
        } catch (const std::exception& e) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            out.error_message = e.what();
            return out;
        } catch (...) {
            auto out = star::transport::WriteResult{};
            out.status = star::transport::WriteStatus::EXCEPTION;
            return out;
        }
    }

    auto write_with_delivery(const star::transport::MessageView& message, int64_t timeout_ms) -> std::future<star::transport::DeliveryResult> override {
        return write_impl(message, nullptr, timeout_ms);
    }

    auto write_serialized_with_delivery(const star::transport::SerializedMessage& message, int64_t timeout_ms)
        -> std::future<star::transport::DeliveryResult> override {
        return write_impl(message.view, &message.serializer, timeout_ms);
    }

  private:
    auto write_impl(const star::transport::MessageView& message, const star::transport::MessageSerializer* serializer, int64_t timeout_ms)
        -> std::future<star::transport::DeliveryResult> {
        static_cast<void>(timeout_ms);
        auto promise = std::make_shared<std::promise<star::transport::DeliveryResult>>();
        auto future = promise->get_future();

        auto serializer_copy = serializer ? std::make_shared<star::transport::MessageSerializer>(*serializer) : nullptr;
        auto fut = std::async(std::launch::async, [this, promise, message, serializer_copy]() {
            try {
                auto begin = std::chrono::steady_clock::now();
                auto bus = bus_.lock();
                if (not bus) {
                    promise->set_value({false, "bus expired", 0});
                    return;
                }
                bus->write_internal(channel_name_, message, serializer_copy ? serializer_copy.get() : nullptr);
                const auto latency_us = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count());
                promise->set_value({true, "inproc delivery complete", latency_us});
            } catch (const std::exception& e) {
                promise->set_value({false, e.what(), 0});
            } catch (...) {
                promise->set_value({false, {}, 0});
            }
        });

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_acks_.emplace_back(std::move(fut));
        }

        return future;
    }

  private:
    std::weak_ptr<NativeMessageBus> bus_;
    std::string channel_name_;
    std::mutex pending_mutex_;
    std::vector<std::future<void>> pending_acks_;
};

#endif  // __NATIVE_WRITER_H__
