#ifndef __MESSAGE_BUS_H__
#define __MESSAGE_BUS_H__

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "basement/core/common_types.h"
#include "basement/schema/type_id.h"
#include "basement/transport/api/correlation_id.h"
#include "basement/transport/common/channel_stats.h"

namespace star::transport {

enum class DropPolicy {
    KEEP_LAST,
    DROP_NEWEST,
};

struct ChannelOptions {
    uint32_t depth{0};
    DropPolicy drop_policy{DropPolicy::KEEP_LAST};
    bool reliable{false};
};

class MessageRecorder;

struct MessageView {
    star::base::TypedConstPtr payload;
    star::schema::SchemaTypeId schema_type_id;
    CorrelationId correlation_id;
};

struct BinaryPayloadView {
    std::shared_ptr<const void> owner;
    const uint8_t* data{nullptr};
    size_t size{0};

    auto bytes() const -> std::span<const uint8_t> { return std::span<const uint8_t>(data, size); }
};

struct MessageSerializer {
    size_t payload_size_hint{0};
    std::function<std::vector<uint8_t>()> encode_binary;
    std::function<size_t(uint8_t*, size_t)> encode_into;
};

struct SerializedMessage {
    MessageView view;
    MessageSerializer serializer;
};

struct ChannelDescriptor {
    std::string name;
    std::string type_name;
    star::schema::SchemaTypeId schema_type_id;
    ChannelOptions options;
};

struct DeliveryResult {
    bool accepted{false};
    std::string message;
    int64_t latency_us{0};
};

struct Port {
    std::string component_name;
    std::string port_name;
    bool is_input{true};
};

enum class WriteStatus {
    OK,
    BUS_EXPIRED,
    CHANNEL_UNKNOWN,
    CHANNEL_NOT_READY,
    SERIALIZER_REQUIRED,
    ENCODE_FAILED,
    SHM_DISABLED,
    SHM_DEGRADED,
    EXCEPTION,
};

inline auto write_status_string(WriteStatus status) -> const char* {
    switch (status) {
        case WriteStatus::OK: return "OK";
        case WriteStatus::BUS_EXPIRED: return "BUS_EXPIRED";
        case WriteStatus::CHANNEL_UNKNOWN: return "CHANNEL_UNKNOWN";
        case WriteStatus::CHANNEL_NOT_READY: return "CHANNEL_NOT_READY";
        case WriteStatus::SERIALIZER_REQUIRED: return "SERIALIZER_REQUIRED";
        case WriteStatus::ENCODE_FAILED: return "ENCODE_FAILED";
        case WriteStatus::SHM_DISABLED: return "SHM_DISABLED";
        case WriteStatus::SHM_DEGRADED: return "SHM_DEGRADED";
        case WriteStatus::EXCEPTION: return "EXCEPTION";
    }
    return "UNKNOWN";
}

struct WriteResult {
    WriteStatus status{WriteStatus::OK};
    bool inproc_ok{false};
    bool shm_ok{false};
    bool shm_required{false};
    std::string error_message;

    auto ok() const -> bool { return status == WriteStatus::OK; }
};

class Writer {
  public:
    virtual ~Writer() = default;
    virtual auto write(const MessageView& message) -> WriteResult = 0;
    virtual auto write_serialized(const SerializedMessage& message) -> WriteResult = 0;
};

class DeliveryWriter : public Writer {
  public:
    virtual ~DeliveryWriter() = default;
    auto write(const MessageView& message) -> WriteResult override = 0;
    auto write_serialized(const SerializedMessage& message) -> WriteResult override = 0;
    virtual auto write_with_delivery(const MessageView& message, int64_t timeout_ms) -> std::future<DeliveryResult> = 0;
    virtual auto write_serialized_with_delivery(const SerializedMessage& message, int64_t timeout_ms) -> std::future<DeliveryResult> = 0;
};

class Reader {
  public:
    using Callback = std::function<void(const MessageView&)>;
    virtual ~Reader() = default;
    virtual auto peek_latest() const -> std::optional<MessageView> { return std::nullopt; }
    virtual auto has_data() const -> bool { return false; }
};

class MessageBus {
  public:
    virtual ~MessageBus() = default;

    virtual void declare_channel(const ChannelDescriptor& descriptor) = 0;
    virtual auto create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> = 0;
    virtual auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> = 0;
    virtual auto create_delivery_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<DeliveryWriter> = 0;
    virtual auto declared_channels() const -> std::vector<std::string> = 0;
    virtual auto channel_stats(const std::string& channel_name) const -> std::optional<ChannelStatsSnapshot> { (void)channel_name; return std::nullopt; }
    virtual auto all_channel_stats() const -> std::vector<ChannelStatsSnapshot> { return {}; }
    virtual auto peek_latest(const std::string& channel_name) const -> std::optional<MessageView> { (void)channel_name; return std::nullopt; }
    virtual auto dump_stats_lines() const -> std::vector<std::string> { return {}; }
};

class RecordableBus {
  public:
    virtual ~RecordableBus() = default;
    virtual void enable_recorder(const std::string& file_path) = 0;
    virtual void enable_record_channel(const std::string& channel) = 0;
    virtual void disable_recorder() = 0;
    virtual void disable_record_channel(const std::string& channel) = 0;
};

}  // namespace star::transport

#endif  // __MESSAGE_BUS_H__
