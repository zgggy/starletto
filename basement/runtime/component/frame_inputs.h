#ifndef __STAR_RUNTIME_COMPONENT_FRAME_INPUTS_H__
#define __STAR_RUNTIME_COMPONENT_FRAME_INPUTS_H__

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/common_types.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/shm/shm_views.h"

namespace star::runtime {

// 一次 process_once 的输入：frame 信息 + **本次触发它的消息**（按端口名）。
// 非 trigger 输入不在里面，继续用 Component::read_latest<T>() 读缓存。
struct FrameInputs {
    int64_t frame_id{0};
    int64_t timestamp_ns{0};
    std::unordered_map<std::string, star::transport::MessageView> messages;

    auto contains(std::string_view port) const -> bool { return find(port) != nullptr; }

    // 端口数很少（通常 1~3），线性比较避免每次 trigger 为 string_view 分配 std::string。
    auto find(std::string_view port) const -> const star::transport::MessageView* {
        for (const auto& [name, message] : messages) {
            if (name.size() == port.size() && std::equal(name.begin(), name.end(), port.begin())) {
                return &message;
            }
        }
        return nullptr;
    }
};

// trigger 回调运行在 poller 线程，process_once 在其后执行；shm 槽可能已经被覆写。
// 把 shm/binary 载荷拷成不可变快照，保证“触发这条消息”就是 process_once 看到的消息。
inline auto snapshot_trigger_message(const star::transport::MessageView& view) -> std::optional<star::transport::MessageView> {
    auto out = view;
    if (const auto slot = view.payload.as<star::transport::ShmSlotView>()) {
        if (not slot->stable()) {
            return std::nullopt;
        }
        auto owner = std::make_shared<std::vector<uint8_t>>();
        if (slot->size > 0) {
            if (slot->data == nullptr) {
                return std::nullopt;
            }
            owner->assign(slot->data, slot->data + slot->size);
        }
        out.payload = star::base::TypedConstPtr::from(
            std::make_shared<star::transport::BinaryPayloadView>(star::transport::BinaryPayloadView{owner, owner->data(), owner->size()}));
        return out;
    }
    // BinaryPayloadView 的 owner 本身不可变；typed inproc 指针保持零拷贝。
    return out;
}

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_FRAME_INPUTS_H__
