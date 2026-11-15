// 插件 ABI 契约测试用 DSO：
//   - 默认变体：导出 star_register_components + 正确的工具链指纹
//   - STAR_TEST_NO_FINGERPRINT：缺指纹（必须被加载器拒绝）
//   - STAR_TEST_WRONG_FINGERPRINT：指纹不一致（必须被加载器拒绝）
#include "basement/core/build_info.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"

namespace {

class AbiProbe final : public star::runtime::Component {
  public:
    auto process_once(const star::runtime::FrameInputs&) -> bool override { return true; }
};

}  // namespace

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    if (registry != nullptr) {
        registry->register_component<AbiProbe>("star::test::AbiProbe");
    }
}

#ifndef STAR_TEST_NO_FINGERPRINT
extern "C" const char* star_toolchain_fingerprint() {
#ifdef STAR_TEST_WRONG_FINGERPRINT
    return "wrong-toolchain-fingerprint";
#else
    return star::base::toolchain_fingerprint();
#endif
}
#endif
