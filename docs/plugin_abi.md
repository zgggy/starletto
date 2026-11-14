# 插件 ABI 契约

> 状态：第一阶段（可检查契约）与安装面（headers/pc/schema 工具）已落地；共享运行时库为下一阶段。
> 适用对象：写 `modules/` 插件的人、需要分发预编译插件的人。

## 1. 当前模型

Starletto 运行时是 **header-only**：`Component`、`MessageBus`、schema 编解码等实现
都编译进每个插件 `.so` 和宿主 `native_runner`。插件通过 `dlopen` 加载，入口是：

```cpp
extern "C" void star_register_components(star::runtime::ComponentRegistry*);
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
```

因此插件与宿主之间传递的是 **C++ ABI**：虚表布局、`std::string`/`std::function`/
`std::shared_ptr` 的实现、异常传播、RTTI 都必须一致。

## 2. 必须一致的项

| 项 | 不一致的后果 |
|---|---|
| 编译器厂商/主版本（clang/gcc） | 虚表布局、名字修饰、异常模型不同 → 崩溃或错误分发 |
| 标准库（libc++ / libstdc++）及版本 | `std::string`/`std::function` 布局不同 → 内存破坏 |
| `_GLIBCXX_USE_CXX11_ABI` / `_LIBCPP_ABI_VERSION` | string/list 布局不同 |
| `NDEBUG`（Release/Debug 混用） | 断言与某些 inline 行为不同 |
| 架构/指针宽度 | 布局与对齐不同 |

**当前不承诺跨编译器插件**。这是 header-only 的代价，不是配置问题。

## 3. 如何检查

`register.cc` 里的 `STAR_EXPORT_TOOLCHAIN_FINGERPRINT()` 会导出一个
`star_toolchain_fingerprint()` 符号，内容由编译器宏生成（编译器、标准库版本、
ABI 宏、`__cplusplus`、指针宽度、NDEBUG）。

`star doctor --deep` / `star check` 会：

1. `dlopen` 插件；
2. `dlsym` `star_register_components`；
3. `dlsym` `star_toolchain_fingerprint` 并与宿主比较；
4. 不一致 → 报错并让门禁失败；缺失 → 告警（旧插件，无法验证）。

`star new component` 生成的 `register.cc` 默认包含该宏。

## 4. 指纹不覆盖什么

- 编译器补丁级差异、未列入指纹的编译选项（如 `-fno-exceptions`、`-fno-rtti`）；
- 未定义行为/ODR 违规；
- 运行期内存模型差异。

指纹是**发现混编**，不是 ABI 稳定性保证。

## 5. 进展与后续阶段

已完成：工具链指纹 + `doctor --deep` 校验；`meson install` 安装 headers、
生成头、schema 工具与 `starletto.pc`；源码树外插件冒烟（`star check` 的
`install-smoke`）。

1. 抽出 `libstar_runtime.so`（core/schema/transport/runtime 的非模板部分），插件链接它；
2. 插件入口改为 C ABI（函数表/`void*`），STL 类型留在库内；
3. `install_headers` + `starletto.pc`，支持源码树外开发插件；
4. 完成后再评估跨编译器兼容。

在此之前，**预编译插件必须与宿主出自同一构建配置**。
