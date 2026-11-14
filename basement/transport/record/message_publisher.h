#ifndef __MESSAGE_PUBLISHER_H__
#define __MESSAGE_PUBLISHER_H__

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "basement/transport/api/message_bus.h"
#include "basement/transport/record/message_replayer.h"

namespace star::transport {

// 把录制条目重新发布回通道。
//
// 回放不重新编码：录制里存的是当时写出的字节，原样写回才能验证「旧数据在新代码里
// 怎么解码」——这正是解码策略（unknown/missing/type_mismatch）存在的场景。
// schema identity 随条目带过去，接收端的类型校验与策略据此生效。
class MessagePublisher final {
  public:
    explicit MessagePublisher(MessageBus& bus) : bus_(&bus) {}

    MessagePublisher(const MessagePublisher&) = delete;
    MessagePublisher& operator=(const MessagePublisher&) = delete;

    auto publish(const MessageReplayer::RecordEntry& entry) -> WriteResult {
        auto writer = writer_for(entry);
        if (not writer) {
            auto out = WriteResult{};
            out.status = WriteStatus::CHANNEL_UNKNOWN;
            out.error_message = "failed to create replay writer for channel: " + entry.channel_name;
            return out;
        }
        auto owner = std::make_shared<std::vector<uint8_t>>(entry.payload);
        auto serialized = SerializedMessage{};
        serialized.view.payload = star::base::TypedConstPtr::from(std::make_shared<BinaryPayloadView>(
            BinaryPayloadView{owner, owner->data(), owner->size()}));
        serialized.view.schema_type_id = entry.schema_type_id;
        serialized.serializer.payload_size_hint = owner->size();
        serialized.serializer.encode_binary = [owner]() { return *owner; };
        return writer->write_serialized(serialized);
    }

    auto published_writers() const -> size_t { return writers_.size(); }

  private:
    auto writer_for(const MessageReplayer::RecordEntry& entry) -> std::shared_ptr<Writer> {
        const auto it = writers_.find(entry.channel_name);
        if (it != writers_.end()) {
            return it->second;
        }
        auto descriptor = ChannelDescriptor{};
        descriptor.name = entry.channel_name;
        descriptor.type_name = entry.type_name;
        descriptor.schema_type_id = entry.schema_type_id;
        auto writer = bus_->create_writer(descriptor);
        if (writer) {
            writers_.emplace(entry.channel_name, writer);
        }
        return writer;
    }

    MessageBus* bus_;
    std::unordered_map<std::string, std::shared_ptr<Writer>> writers_;
};

}  // namespace star::transport

#endif  // __MESSAGE_PUBLISHER_H__
