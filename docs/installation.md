# 安装与源码树外开发

> 适用于：基于已安装的 Starletto 开发自己的模块/插件，不改本仓库。
> 当前阶段：headers + 生成的 builtin 头 + schema 工具 + pkg-config 已安装。
> 插件按 header-only C++ 编译，必须与宿主同工具链（见 [plugin_abi.md](plugin_abi.md)）。

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
| `lib/pkgconfig/starletto.pc` | `-I${includedir}/starletto -std=c++20` |
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

## 3. 注意事项

- 插件与宿主必须同工具链/标准库，且必须导出工具链指纹；`doctor --deep`
  与加载器都会强制校验（见 [plugin_abi.md](plugin_abi.md)）。
- 业务模块（setting/planning/control）的 schema **不安装**：它们是模块私有契约，
  源码树外模块应定义自己的 schema。
- `pkg-config` 文件已生成；开发机若未安装 pkg-config，可直接用 `-I` 编译。
