# 插件 ABI 契约

> 状态：**终态（2026-11-14）**。只有一种插件模型：C++ header-only +
> 同工具链。C ABI 双入口、`libstar_runtime` 过渡库、旧插件兼容路径均已删除。

## 1. 唯一入口

每个插件 `.so` 必须导出两个 C 符号：

```cpp
extern "C" void star_register_components(star::runtime::ComponentRegistry*);
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
```

`register.cc` 里同时完成组件注册与 schema 注册：

```cpp
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    my::generated::MyMsg::register_schema(*registry->schema_registry());
    registry->register_component<MyNode>("my_module::MyNode");
}
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
```

加载器只认这一个入口：

- `star_register_components` 缺失 → 加载失败；
- `star_toolchain_fingerprint` 缺失 → 加载失败（不是警告）；
- 指纹与宿主不一致 → 加载失败。

没有 fallback、没有未版本化插件、没有第二套组件模型。

## 2. 为什么终态是 C++，而不是 C ABI

`Component` 的端口是模板（`read_latest<T>`/`write<T>`），消息解码依赖生成类型的
编译期信息；配置、服务、schema 注册也都建立在 C++ 类型之上。纯 C ABI 只能传
原始字节，无法表达这套组件模型。

2026-11-14 曾实现过 C ABI v1（`star_plugin_entry_v1`）作为第二入口，结论是：

- 它只能覆盖「字节过滤/转发」这一小类组件，其余组件仍必须走 C++；
- 两套入口意味着两套组件模型、两套诊断、两套生命周期，属于架构分裂；
- 跨编译器预编译插件不是当前运行时能兑现的承诺——真要跨语言，前置是运行期
  schema 描述符 + C 代码生成，那是新架构，不是兼容层。

因此 C ABI v1 与 `libstar_runtime` 过渡库已整体撤销，不再以「新入口 + 旧入口
并存」的方式演进。

## 3. 必须一致的项

插件与宿主之间传递的是 C++ ABI：虚表布局、`std::string`/`std::function`/
`std::shared_ptr` 的实现、异常传播、RTTI 都必须一致。

| 项 | 不一致的后果 |
|---|---|
| 编译器厂商/主版本（clang/gcc） | 虚表布局、名字修饰、异常模型不同 → 崩溃或错误分发 |
| 标准库（libc++ / libstdc++）及版本 | `std::string`/`std::function` 布局不同 → 内存破坏 |
| `_GLIBCXX_USE_CXX11_ABI` / `_LIBCPP_ABI_VERSION` | string/list 布局不同 |
| `NDEBUG`（Release/Debug 混用） | 断言与某些 inline 行为不同 |
| 架构/指针宽度 | 布局与对齐不同 |

工具链指纹把这些项变成可检查字符串；它不覆盖未列入的编译选项
（如 `-fno-exceptions`、`-fno-rtti`），也不替代 ABI 纪律。

## 4. 检查点

| 位置 | 行为 |
|---|---|
| `ComponentDlHelper::load_dynamic_library` | 运行时强制校验入口与指纹；失败抛 `RuntimeError` |
| `star doctor --deep` | 预检 `star_register_components` + 指纹；缺失/不一致都让门禁失败 |
| `star check` | 模块 doctor、runtime-smoke、install-smoke 都走同一契约 |

回放工具的 `--plugin` 也要求同样的 C++ 入口（它需要插件注册 schema）。

## 5. 生命周期

- 插件 `.so` 由宿主 `DlHandle` 保活，必须比组件实例和 `ComponentRegistry` 活得久；
- **`ComponentRegistry` 必须先于插件的 `DlHandle` 析构**：`register_component<T>`
  生成的工厂闭包代码位于插件 DSO 内，`dlclose` 后再析构 registry 会调用已卸载
  代码；
- `NativeComponentHost` 的成员声明顺序已满足该约束；外部宿主必须照此排列。

## 6. 共享库

当前**不提供** `libstar_runtime`。header-only 是插件 ABI 的终态，不以
「空壳共享库」或「静态/共享双链接」制造过渡状态。把非模板实现整体下沉到
共享库是独立重构，完成前不会以半成品形式发布。

`starletto.pc` 只提供 include 路径与 `-std=c++20`；源码树外插件的编译方式见
[installation.md](installation.md)。
