# 安装与源码树外开发

> 适用于：基于已安装的 Starletto 开发自己的模块/插件，不改本仓库。
> 当前阶段：headers + 生成的 builtin 头 + schema 工具 + pkg-config + 
> `libstar_runtime`（C ABI 适配层）已安装。C++ 插件仍按 header-only 编译；
> 纯 C 插件只需要 `basement/plugin/star_plugin_abi.h`。

## 1. 构建与安装

```bash
meson setup build --prefix=/opt/starletto
meson compile -C build
meson install -C build
```

安装树：

| 路径 | 内容 |
|---|---|
| `include/starletto/basement/...` | 运行时公共头（`core/schema/transport/runtime`） |
| `include/starletto/libraries/...` | 算法/通用库头（含 vendored 第三方） |
| `include/starletto/basement/schema/builtin/**.s.h` | 内置 schema 的生成头（`component.h` 依赖） |
| `share/starletto/schema/schema_codegen.py` | schema 生成器（源码树外可用） |
| `share/starletto/schema/codegen/*.py` | 生成器实现模块 |
| `lib/libstar_runtime.*` | C ABI 适配层（`Component` 适配、ABI 版本探测） |
| `include/starletto/basement/plugin/star_plugin_abi.h` | 纯 C 插件 ABI（任意 C 编译器） |
| `lib/pkgconfig/starletto.pc` | `-I${includedir}/starletto -std=c++20 -lstar_runtime` |
| `bin/native_runner`、`modules/` | 宿主与内置模块 |

## 2. 源码树外插件

```bash
PREFIX=/opt/starletto
CFLAGS="$(pkg-config --cflags starletto)"   # 或 -I$PREFIX/include/starletto -std=c++20
```

1. 定义自己的 schema：

```bash
python3 "$PREFIX/share/starletto/schema/schema_codegen.py" gen \
    --schema my_module/schema/my_msg.schema \
    --output my_module/my_msg.s.h
```

2. 写组件并编译：

```cpp
// my_module/register.cc
#include "basement/core/build_info.h"
#include "basement/runtime/component/component.h"
#include "my_msg.s.h"

class MyNode final : public star::runtime::Component {
  public:
    auto process_once() -> bool override { return true; }
};

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    if (registry->schema_registry() != nullptr) {
        star::generated::MyMsg::register_schema(*registry->schema_registry());
    }
    registry->register_component<MyNode>("my_module::MyNode");
}
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
```

```bash
c++ -std=c++20 -fPIC -shared -I"$PREFIX/include/starletto" -Imy_module \
    my_module/register.cc -o my_module/libmy.so -pthread
```

3. `module.conf` + `component.conf` 指向 `libmy.so`，用任意 `native_runner` 运行：

```bash
native_runner --doctor --deep my_module/module.conf
native_runner --frames=0 my_module/module.conf
```

4. 冒烟脚本（仓库内）：

```bash
tools/tests/oot_plugin_smoke.sh build
```

它会在临时 DESTDIR 下安装、生成 schema、编译插件、`doctor --deep` 并运行；
`star check` 已把它作为 `install-smoke` 门禁（15/15）。

## 3. 纯 C ABI 插件（可选）

需要跨编译器分发、或插件本身就用 C 写时，导出 `star_plugin_entry_v1` 即可，
不链接 `libstar_runtime`：

```bash
cc -std=c11 -O2 -fPIC -shared -I"$PREFIX/include/starletto" \
   my_module/register.c -o my_module/libmy.so
```

插件只依赖 `basement/plugin/star_plugin_abi.h` 与 libc；能力边界（原始字节端口、
不能注册 schema/读配置）见 [plugin_abi.md](plugin_abi.md)。


## 4. 注意事项

- **C++ 插件**与宿主必须同工具链/标准库（`doctor --deep` 会比对工具链指纹，
  见 [plugin_abi.md](plugin_abi.md)）。
- **C ABI 插件**用任意 C 编译器即可；`doctor --deep` 只校验 ABI 版本与
  descriptor，不比对工具链。
- 业务模块（setting/planning/control）的 schema **不安装**：它们是模块私有契约，
  源码树外模块应定义自己的 schema。
- `pkg-config` 文件已生成；开发机若未安装 pkg-config，可直接用 `-I` 编译。
