#ifndef __STAR_RUNTIME_COMPONENT_TYPED_COMPONENT_H__
#define __STAR_RUNTIME_COMPONENT_TYPED_COMPONENT_H__

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "basement/runtime/component/component.h"

namespace star::runtime {

// 编译期字符串，作为 TriggerInput 的端口名模板参数。
template <std::size_t N>
struct FixedString {
    char value[N]{};

    constexpr FixedString(const char (&text)[N]) {
        for (std::size_t i = 0; i < N; ++i) {
            value[i] = text[i];
        }
    }

    constexpr auto view() const -> std::string_view { return std::string_view{value, N - 1}; }
};

template <std::size_t N>
FixedString(const char (&)[N]) -> FixedString<N>;

// TriggerInput：一个 trigger 端口 + 它的消息类型。
// 端口名是编译期常量，类型只写这一处；业务 process_once 用 const auto& 参数。
template <typename T, FixedString Name>
struct TriggerInput {
    using type = T;
    static constexpr auto name = Name;
};

// 框架边界仍是唯一的 Component::process_once(FrameInputs)；这里把 TriggerInput...
// 逐个解码成 typed 参数，再调用派生类自己的 process_once(const auto&...)。
//
// 用法：
//   class Motion final : public TypedComponent<
//       Motion,
//       TriggerInput<Prediction, "prediction">,
//       TriggerInput<Pose, "localization">,
//       TriggerInput<Chassis, "chassis">> {
//     public:
//       auto process_once(const auto& prediction, const auto& pose,
//                         const auto& chassis) -> bool;
//   };
template <typename Derived, typename... TriggerTypes>
class TypedComponent : public Component {
    static_assert(sizeof...(TriggerTypes) > 0, "TypedComponent requires at least one TriggerInput");

  public:
    auto process_once(const FrameInputs& raw) -> bool final {
        return invoke_typed(raw, std::index_sequence_for<TriggerTypes...>{});
    }

    auto declared_trigger_ports() const -> std::span<const std::string_view> final {
        static constexpr std::array<std::string_view, sizeof...(TriggerTypes)> ports{TriggerTypes::name.view()...};
        return ports;
    }

  private:
    template <std::size_t... I>
    auto invoke_typed(const FrameInputs& raw, std::index_sequence<I...>) -> bool {
        auto messages = std::tuple{decode_one<TriggerTypes>(raw)...};
        constexpr std::array<std::string_view, sizeof...(I)> ports{TriggerTypes::name.view()...};

        auto missing = std::string{};
        ((std::get<I>(messages) ? (void)0 : (void)(missing = std::string(ports[I]))), ...);
        if (not missing.empty()) {
            throw star::excep::RuntimeError("TypedComponent: missing trigger message for port=", missing);
        }
        return static_cast<Derived*>(this)->process_once(*std::get<I>(messages)...);
    }

    template <typename TriggerT>
    auto decode_one(const FrameInputs& raw) -> std::shared_ptr<const typename TriggerT::type> {
        return this->template trigger_input<typename TriggerT::type>(raw, TriggerT::name.view());
    }
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_TYPED_COMPONENT_H__
