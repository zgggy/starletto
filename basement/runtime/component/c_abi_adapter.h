#ifndef __STAR_RUNTIME_COMPONENT_C_ABI_ADAPTER_H__
#define __STAR_RUNTIME_COMPONENT_C_ABI_ADAPTER_H__

#include <memory>
#include <string>

#include "basement/plugin/star_plugin_abi.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"

namespace star::runtime {

// 纯 C ABI 插件描述 → Component。实现位于 libstar_runtime.so：
// C 插件不链接、不包含任何 C++ 运行时头；适配与类型擦除全部留在宿主编译单元。
auto make_c_abi_component(const star_plugin_component_v1* component) -> std::unique_ptr<Component>;

// 校验 descriptor（ABI 版本/空指针/重复类名）并逐组件注册工厂。
// 插件 .so 必须比 registry 中的工厂活得久（宿主用 DlHandle 保活）。
void register_c_abi_plugin_components(const star_plugin_v1* plugin, ComponentRegistry& registry);

class CAbiComponentAdapter final : public Component {
  public:
    explicit CAbiComponentAdapter(const star_plugin_component_v1* component);
    ~CAbiComponentAdapter() override;

    CAbiComponentAdapter(const CAbiComponentAdapter&) = delete;
    auto operator=(const CAbiComponentAdapter&) -> CAbiComponentAdapter& = delete;

    void init() override;
    auto process_once() -> bool override;
    void shutdown() override;

  private:
    static auto read_latest_thunk(void* ctx, const char* port, void* buffer, size_t capacity, size_t* out_size, const char** schema_name, int64_t* schema_version,
                                  uint64_t* schema_hash) -> int;
    static auto publish_thunk(void* ctx, const char* port, const void* data, size_t size, const char* schema_name, int64_t schema_version, uint64_t schema_hash) -> int;
    static auto last_error_thunk(void* ctx) -> const char*;

    auto read_latest(const char* port, void* buffer, size_t capacity, size_t* out_size, const char** schema_name, int64_t* schema_version, uint64_t* schema_hash) -> int;
    auto publish(const char* port, const void* data, size_t size, const char* schema_name, int64_t schema_version, uint64_t schema_hash) -> int;

    void set_error(std::string message);

    const star_plugin_component_v1* component_{nullptr};
    void* self_{nullptr};
    star_component_api_v1 api_{};
    std::string last_error_;
    std::string last_schema_name_;
    bool initialized_{false};
    bool shutdown_called_{false};
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_C_ABI_ADAPTER_H__
