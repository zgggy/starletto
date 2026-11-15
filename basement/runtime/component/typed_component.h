#ifndef __STAR_RUNTIME_COMPONENT_TYPED_COMPONENT_H__
#define __STAR_RUNTIME_COMPONENT_TYPED_COMPONENT_H__

#include "basement/runtime/component/component.h"

namespace star::runtime {

// 框架边界仍是唯一的 process_once(FrameInputs)；这里把「trigger 消息 → 组件自己的
// 类型化输入结构」的转换收敛到组件侧的一次 make_inputs()，业务方法就不必再写
// trigger_input<T>(inputs, "port") 这种模板+字符串取值。
//
// 用法：
//   struct MotionInputs { std::shared_ptr<const Prediction> prediction; ... };
//   class Motion final : public TypedComponent<Motion, MotionInputs> {
//     public:
//       auto make_inputs(const FrameInputs& raw) -> MotionInputs;
//       auto process_once(const MotionInputs& inputs) -> bool;
//   };
//
// 组件之间的 process_once 参数可以不同；宿主/调度器只看到 Component 虚函数，
// 适配由 TypedComponent 完成。
template <typename _Derived, typename _Inputs>
class TypedComponent : public Component {
  public:
    auto process_once(const FrameInputs& raw) -> bool final {
        auto* self = static_cast<_Derived*>(this);
        return self->process_once(self->make_inputs(raw));
    }
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_TYPED_COMPONENT_H__
