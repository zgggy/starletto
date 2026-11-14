#!/usr/bin/env python3
"""生成模块/组件骨架（`star new module|component`）。

目标不是「生成一堆文件」，而是把最容易错的三处一次写对：
  1. component.conf 的 component_library / component_class；
  2. register.cc 的 star_register_components 签名；
  3. 父 module.conf 的 components 列表与 meson 的 subdir 接线。
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

IDENT = re.compile(r"^[a-z][a-z0-9_]*$")


def fail(message: str) -> "NoReturn":
    print(f"[E] {message}", file=sys.stderr)
    raise SystemExit(1)


def pascal_case(name: str) -> str:
    return "".join(part.capitalize() for part in re.split(r"[_\-]+", name) if part)


def write_new(path: pathlib.Path, content: str) -> None:
    if path.exists():
        fail(f"refusing to overwrite existing file: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    print(f"  created {path}")


def ensure_line(path: pathlib.Path, line: str) -> None:
    text = path.read_text(encoding="utf-8")
    if line in text.splitlines():
        return
    if text and not text.endswith("\n"):
        text += "\n"
    path.write_text(text + line + "\n", encoding="utf-8")


def module_conf(name: str) -> str:
    return f"""module {{
    name = {name}
    version = 0.1.0
}}

logging {{
    level = INFO
}}

components []
"""


def module_meson(name: str) -> str:
    return f"""{name}_inc = include_directories(
    '.',
    '..',
    '../..',
)

{name}_dep = declare_dependency(
    include_directories: {name}_inc,
    dependencies: [
        star_core_dep,
        runtime_api_dep,
        transport_api_dep,
        manifest_dep,
        libraries_dep,
    ],
)

install_data(
    'module.conf',
    install_dir: 'modules/{name}',
)
"""


def component_conf(module: str, name: str, cls: str) -> str:
    return f"""global {{
    process_group = {module}
}}

component {{
    name = {name}
    type = {module}
    component_library = lib{name}.so
    component_class = star::{module}::{cls}
}}

trigger {{
    cycle_ms = 100
}}
"""


def processor_header(module: str, name: str, cls: str) -> str:
    guard = f"__STAR_{module.upper()}_{name.upper()}_PROCESSOR_H__"
    return f"""#ifndef {guard}
#define {guard}

namespace star::{module} {{

// 纯算法部分：不依赖运行时、不做 IO，方便单测和复用。
struct {cls}Input {{
    double value{{0.0}};
}};

struct {cls}Output {{
    double value{{0.0}};
}};

class {cls}Processor {{
  public:
    auto process(const {cls}Input& input) -> {cls}Output {{
        auto output = {cls}Output{{}};
        output.value = input.value * 2.0;
        return output;
    }}
}};

}}  // namespace star::{module}

#endif  // {guard}
"""


def component_header(module: str, name: str, cls: str) -> str:
    guard = f"__STAR_{module.upper()}_{name.upper()}_COMPONENT_H__"
    return f"""#ifndef {guard}
#define {guard}

#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/{module}/{name}/{name}_processor.h"

namespace star::{module} {{

// 适配层：只做「取输入 → 调算法 → 写输出」，算法细节留在 processor 里。
// 端口名（read_latest/write 的字符串）必须与 component.conf 的键名一致；
// 本模板没有端口，需要数据时按 docs/component_guide.md 增加 readers/writers。
class {cls}Component final : public star::runtime::Component {{
  public:
    auto process_once() -> bool override {{
        const auto& frame = context()->current_frame();
        auto input = {cls}Input{{}};
        const auto output = processor_.process(input);
        logD << "{cls}Component frame=" << frame.frame_id << ", value=" << output.value;
        return true;
    }}

  private:
    {cls}Processor processor_;
}};

inline auto register_native_components(star::runtime::ComponentRegistry& registry) -> void {{
    registry.register_component<{cls}Component>("star::{module}::{cls}");
    registry.register_component<{cls}Component>("star::{module}::{cls}Component");
}}

}}  // namespace star::{module}

#endif  // {guard}
"""


def register_cc(module: str, name: str) -> str:
    return f"""#include "basement/runtime/component/component_registry.h"
#include "modules/{module}/{name}/{name}_component.h"

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {{
    star::{module}::register_native_components(*registry);
}}
"""


def component_meson(module: str, name: str) -> str:
    return f"""{name}_sources = files(
    '{name}_component.h',
    '{name}_processor.h',
)

{name}_dep = declare_dependency(
    sources: {name}_sources,
    dependencies: [{module}_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/{module}/{name}',
)

shared_library(
    '{name}',
    files('register.cc'),
    dependencies: [{name}_dep],
    name_suffix: 'so',
    install: true,
    install_dir: 'modules/{module}/{name}',
)
"""


def component_readme(module: str, name: str, cls: str) -> str:
    return f"""# {module}/{name}

由 `star new component` 生成的最小骨架。

## 文件职责

| 文件 | 作用 |
|---|---|
| `component.conf` | 端口/触发/类名声明（`{cls}`） |
| `{name}_component.h` | 适配层：取输入、调算法、写输出 |
| `{name}_processor.h` | 纯算法（建议在这里写单测） |
| `register.cc` | 插件入口（`star_register_components`） |
| `meson.build` | 构建与安装规则 |

## 下一步

1. 在 `component.conf` 增加 `readers`/`writers`，键名与代码里的端口字符串保持一致；
2. 在 `{name}_component.h` 用 `read_latest<T>("port")` / `write("port", msg)` 接线；
3. 跑 `star build && star run {module} && star check`；
4. 详细规则见 `docs/component_guide.md`。

## 三个名字必须一致

- `component.conf` 的 `component_library` = meson 的 `shared_library('{name}', ...)` 产物；
- `component.conf` 的 `component_class` = `register_native_components` 注册的字符串；
- `readers/writers` 的键名 = 代码里 `read_latest("...")` / `write("...", ...)` 的字符串。
"""


def cmd_module(root: pathlib.Path, name: str) -> int:
    if not IDENT.match(name):
        fail(f"invalid module name '{name}' (expect [a-z][a-z0-9_]*)")
    module_dir = root / "modules" / name
    if module_dir.exists():
        fail(f"module already exists: {module_dir}")

    print(f"creating module {name}")
    write_new(module_dir / "module.conf", module_conf(name))
    write_new(module_dir / "meson.build", module_meson(name))
    ensure_line(root / "modules" / "meson.build", f"subdir('{name}')")
    print(f"[OK] module {name} created; add a component with: star new component {name}/<name>")
    return 0


def register_component_in_module(module_conf_path: pathlib.Path, entry: str) -> None:
    text = module_conf_path.read_text(encoding="utf-8")
    if re.search(r"^components\s*\[.*\]\s*$", text, flags=re.MULTILINE):
        def add(match: "re.Match[str]") -> str:
            line = match.group(0).strip()
            body = line[line.index("["):]
            items = body.strip("[]").strip()
            new_body = f"[{items}, {entry}]" if items else f"[{entry}]"
            return f"components {new_body}"

        text = re.sub(r"^components\s*\[.*\]\s*$", add, text, flags=re.MULTILINE)
    else:
        if text and not text.endswith("\n"):
            text += "\n"
        text += f"\ncomponents [{entry}]\n"
    if not text.endswith("\n"):
        text += "\n"
    module_conf_path.write_text(text, encoding="utf-8")


def cmd_component(root: pathlib.Path, target: str) -> int:
    if "/" not in target:
        fail("expected <module>/<name>")
    module, name = target.split("/", 1)
    if not IDENT.match(module) or not IDENT.match(name):
        fail(f"invalid names '{module}/{name}' (expect [a-z][a-z0-9_]*)")
    cls = pascal_case(name)
    module_dir = root / "modules" / module
    if not (module_dir / "module.conf").exists():
        fail(f"module not found: modules/{module}/module.conf (run `star new module {module}` first)")
    component_dir = module_dir / name
    if component_dir.exists():
        fail(f"component already exists: {component_dir}")

    print(f"creating component {module}/{name} (class star::{module}::{cls})")
    write_new(component_dir / "component.conf", component_conf(module, name, cls))
    write_new(component_dir / f"{name}_processor.h", processor_header(module, name, cls))
    write_new(component_dir / f"{name}_component.h", component_header(module, name, cls))
    write_new(component_dir / "register.cc", register_cc(module, name))
    write_new(component_dir / "meson.build", component_meson(module, name))
    write_new(component_dir / "README.md", component_readme(module, name, cls))
    ensure_line(module_dir / "meson.build", f"subdir('{name}')")
    register_component_in_module(module_dir / "module.conf", f"{name}/component.conf")
    print(f"[OK] component {module}/{name} created; run `star build` next")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(prog="star new")
    parser.add_argument("--root", default=None, help="项目根目录（默认按脚本位置推断）")
    sub = parser.add_subparsers(dest="command", required=True)

    module_parser = sub.add_parser("module")
    module_parser.add_argument("name")

    component_parser = sub.add_parser("component")
    component_parser.add_argument("target", help="<module>/<name>")

    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve() if args.root else pathlib.Path(__file__).resolve().parents[2]
    if args.command == "module":
        return cmd_module(root, args.name)
    return cmd_component(root, args.target)


if __name__ == "__main__":
    raise SystemExit(main())
