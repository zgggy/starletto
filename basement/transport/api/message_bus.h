#ifndef __MESSAGE_BUS_H__
#define __MESSAGE_BUS_H__

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "basement/core/common_types.h"
#include "basement/schema/type_id.h"
#include "basement/transport/common/channel_stats.h"

namespace star::transport {

enum class DropPolicy {
    KEEP_LAST,
    DROP_NEWEST,
};

enum class AckMode {
    NO_ACK,
    BLOCKING,
    FIRE_AND_FORGET,
};

struct ChannelOptions {
    uint32_t depth{0};
    DropPolicy drop_policy{DropPolicy::KEEP_LAST};
    bool reliable{false};
    int64_t ack_timeout_ms{0};
};

class MessageRecorder;

struct MessageView {
    star::base::TypedConstPtr data;
    star::schema::SchemaTypeId schema_type_id;
    std::function<std::vector<uint8_t>()> encode_binary;
    uint64_t correlation_id{0};
};

struct ChannelDescriptor {
    std::string name;
    std::string type_name;
    star::schema::SchemaTypeId schema_type_id;
    ChannelOptions options;
};

struct AckResult {
    bool accepted{false};
    std::string message;
    int64_t latency_us{0};
};

struct Port {
    std::string component_name;
    std::string port_name;
    bool is_input{true};
};

class Writer {
  public:
    virtual ~Writer() = default;
    virtual void write(const MessageView& message) = 0;
};

class AckWriter : public Writer {
  public:
    using TimeoutCallback = std::function<void()>;
    virtual ~AckWriter() = default;
    void write(const MessageView& message) override = 0;
    virtual auto write_with_ack(const MessageView& message, int64_t timeout_ms) -> std::future<AckResult> = 0;
    virtual void set_timeout_callback(TimeoutCallback cb) = 0;
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
    virtual auto create_ack_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<AckWriter> = 0;
    virtual auto dump_stats_lines() const -> std::vector<std::string> = 0;
    virtual auto declared_channels() const -> std::vector<std::string> = 0;
    virtual auto channel_stats(const std::string& channel_name) const -> std::optional<ChannelStatsSnapshot> { (void)channel_name; return std::nullopt; }
    virtual auto all_channel_stats() const -> std::vector<ChannelStatsSnapshot> { return {}; }

    virtual void enable_recorder(const std::string& file_path) { (void)file_path; }
    virtual void enable_record_channel(const std::string& channel) { (void)channel; }
};

}  // namespace star::transport

#endif  // __MESSAGE_BUS_H__
