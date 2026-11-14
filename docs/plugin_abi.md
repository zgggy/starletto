# 插件 ABI 契约

> 状态：C ABI v1 与 C++ header-only 两条插件路径并存；`libstar_runtime` 已作为
> C ABI 适配层落地（2026-11-14）。
> 适用对象：写 `modules/` 插件的人、需要分发预编译插件的人。

## 1. 两种插件入口

宿主按以下顺序探测：

| 优先级 | 入口符号 | 编译要求 | 能力 |
|---|---|---|---|
| 1 | `star_plugin_entry_v1` | **纯 C 头**，任意 C 编译器；不要求与宿主同工具链 | 原始字节端口（读/写） |
| 2 | `star_register_components` | C++，必须与宿主同编译器/标准库/ABI | 完整 `Component` API（schema 编解码、服务、配置…） |

两个符号都存在时以 C ABI 为准。

### 1.1 C ABI v1（纯 C 插件）

头文件：`basement/plugin/star_plugin_abi.h`。它只依赖 `<stddef.h>`/`<stdint.h>`，
不含任何 C++ 类型、模板或 STL：

```c
#include "basement/plugin/star_plugin_abi.h"

typedef struct { int calls; } node;
static node g_node;

static void* node_create(void) { return &g_node; }
static void node_destroy(void* self) { (void)self; }
static int node_process(void* self) { ((node*)self)->calls += 1; return 0; }

static const star_plugin_component_v1 k_components[] = {
    {"my_module::MyNode", node_create, node_destroy, NULL, node_process, NULL},
};
static const star_plugin_v1 k_plugin = {
    STAR_PLUGIN_ABI_VERSION, "my_c_plugin", k_components, 1,
};
const star_plugin_v1* star_plugin_entry_v1(void) { return &k_plugin; }
```

```bash
cc -std=c11 -O2 -fPIC -shared -I"$PREFIX/include/starletto" \
   my_module/register.c -o my_module/libmy.so
```

这就是 ABI 边界的意义：插件是 `cc` 编出来的，**不链接 `libstar_runtime`、
不链接 libc++**，宿主用 `dlopen` 加载即可。`star check` 的 `install-smoke`
会用安装出来的头编译一个这样的插件并实际运行。

#### 端口 API

组件通过 `star_component_api_v1` 的 `read_latest`/`publish` 收发**原始 wire 字节**：

```c
/* 0=成功, 1=暂无数据, 2=buffer 太小(所需大小已写入 out_size), <0=错误 */
int read_latest(void* ctx, const char* port, void* buffer, size_t capacity,
                size_t* out_size, const char** schema_name,
                int64_t* schema_version, uint64_t* schema_hash);

/* 0=成功, <0=失败；详情用 last_error(ctx) 取 */
int publish(void* ctx, const char* port, const void* data, size_t size,
            const char* schema_name, int64_t schema_version, uint64_t schema_hash);
```

- 身份（name/version/hash）随 shm 帧头传递，接收端仍按 schema 校验；
  C 插件自己负责 payload 的字节布局。
- `read_latest` 典型用法是两次调用：先 `buffer=NULL, capacity=0` 取大小，
  返回 2 后分配再读；空 payload 第一次就返回 0（`out_size=0`）。
- `schema_name` 指针有效期到下一次 `read_latest`/`process` 返回，不要长期保存。
- `frame_id`/`timestamp_ns` 是当前调度帧，每次 `process` 前由宿主刷新。

#### 生命周期与限制

`create → init → process* → shutdown → destroy`，均在宿主调度线程上调用。
`create` 必须返回非空指针；`destroy` 必须提供；`init/process/shutdown` 可空。
descriptor 必须静态存储，且插件 `.so` 必须比宿主中的组件工厂活得久
（宿主用 `DlHandle` 保活）。

v1 不提供：schema 注册、`component.conf` 配置读取、类型化消息、服务调用封装。
需要这些能力时使用 C++ 插件；C 插件适合做字节级过滤/转发/桥接。

另一个限制：`read_latest` 只能拿到**可序列化视图**（shm 槽或 `BinaryPayloadView`）。
若同进程内的 C++ 组件以 typed 指针写入（单进程快路径），C 插件读该通道会得到
「暂无数据」；把该通道放到跨进程路由/录制路径上即可得到字节视图。

### 1.2 C++ 插件（header-only）

```cpp
extern "C" void star_register_components(star::runtime::ComponentRegistry*);
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
```

插件与宿主之间传递的是 C++ ABI：虚表布局、`std::string`/`std::function`/
`std::shared_ptr` 的实现、异常传播、RTTI 都必须一致。必须一致的项不变：

| 项 | 不一致的后果 |
|---|---|
| 编译器厂商/主版本（clang/gcc） | 虚表布局、名字修饰、异常模型不同 → 崩溃或错误分发 |
| 标准库（libc++ / libstdc++）及版本 | `std::string`/`std::function` 布局不同 → 内存破坏 |
| `_GLIBCXX_USE_CXX11_ABI` / `_LIBCPP_ABI_VERSION` | string/list 布局不同 |
| `NDEBUG`（Release/Debug 混用） | 断言与某些 inline 行为不同 |
| 架构/指针宽度 | 布局与对齐不同 |

**C++ 插件仍不承诺跨编译器**；需要跨编译器分发时写 C ABI 插件。

## 2. 如何检查

`star doctor --deep` / `star check` 会 `dlopen` 每个插件：

- 有 `star_plugin_entry_v1`：校验 ABI 版本与 descriptor（类名/create/destroy/
  component_count），通过后打印 `C ABI plugin v1: ...`。**不比对工具链指纹**。
- 否则检查 `star_register_components`，再比对 `star_toolchain_fingerprint()`；
  不一致让门禁失败，缺失只告警（旧插件无法验证）。

回放工具的 `--plugin` 只为注册 schema：C ABI v1 插件没有 schema，遇到时打印
`[W] C ABI plugin has no schema registration (v1), skipped` 并继续。

## 3. 共享库 `libstar_runtime`

`libstar_runtime.so` 当前承载：

- `CAbiComponentAdapter`（把 C descriptor 适配成 `Component`）
- `register_c_abi_plugin_components()`（校验 + 注册工厂）
- `star_runtime_plugin_abi_version()`（外部工具探测 ABI 版本）

**core/schema/transport 仍是 header-only**；这不是完整运行时库，而是
「非模板实现下沉」的第一块。`starletto.pc` 的 `Libs` 已带 `-lstar_runtime`，
C++ 插件可链接它（不链接也能按 header-only 工作）；C 插件只包含头文件，不需要链接。

## 4. 为什么没有把 Component 整体改成 C ABI

评估结论（2026-11-14）：暂不做，原因：

1. `Component` 的端口是模板（`read_latest<T>`/`write<T>`），解码依赖生成类型的
   编译期信息；要让 C ABI 覆盖完整组件模型，需要 codegen 输出运行期 schema
   描述符并让库做表驱动编解码，是独立的大工程。
2. 把 `Component` 拆成 Lifecycle/Ports/Config/Service 会改变所有子类的 API
   契约，但当前 599 行的聚合没有造成正确性缺陷；拆分收益主要是形式上的，
   与「底层先稳定」的目标不符。
3. 错误模型维持现状：编程错误抛异常，运行时状况用 `WriteResult` 状态码；
   C ABI 边界把二者统一映射为 `int` + `last_error()`，不引入第三套模型。

后续若要做完整 C ABI，优先级是「运行期 schema 描述符 + 配置视图」，而不是
先拆 `Component`。

## 5. 进展

- ✅ 工具链指纹 + `doctor --deep` 校验（2026-11-14）
- ✅ headers/生成头/schema 工具/pkg-config 安装（2026-11-14）
- ✅ C ABI v1 + `libstar_runtime` + 纯 C 插件单测/安装面冒烟（2026-11-14）
- ⏳ 运行期 schema 描述符、配置视图（C ABI v2 的前置）
