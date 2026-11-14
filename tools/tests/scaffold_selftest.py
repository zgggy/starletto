#!/usr/bin/env python3
"""`star new` 脚手架行为自检：结构、三处接线、错误路径。"""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCAFFOLD = ROOT / "tools" / "new_component" / "scaffold.py"

_failures: list[str] = []


def check(name: str, condition: bool, detail: str = "") -> None:
    if condition:
        print(f"[ OK ] {name}")
        return
    print(f"[FAIL] {name}" + (f": {detail}" if detail else ""))
    _failures.append(name)


def run(root: pathlib.Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(SCAFFOLD), "--root", str(root), *args],
        capture_output=True,
        text=True,
    )


with tempfile.TemporaryDirectory() as tmp:
    root = pathlib.Path(tmp)
    (root / "modules").mkdir()
    (root / "modules" / "meson.build").write_text("")

    result = run(root, "module", "demo_mod")
    check("module scaffold succeeds", result.returncode == 0, result.stderr)
    module_conf = root / "modules" / "demo_mod" / "module.conf"
    check("module.conf created", module_conf.exists())
    check("module build list updated", "subdir('demo_mod')" in (root / "modules" / "meson.build").read_text())

    result = run(root, "component", "demo_mod/demo_node")
    check("component scaffold succeeds", result.returncode == 0, result.stderr)
    component_dir = root / "modules" / "demo_mod" / "demo_node"
    for file_name in ["component.conf", "demo_node_component.h", "demo_node_processor.h", "register.cc", "meson.build", "README.md"]:
        check(f"{file_name} created", (component_dir / file_name).exists())

    conf_text = (component_dir / "component.conf").read_text()
    check("library name matches meson product", "component_library = libdemo_node.so" in conf_text)
    check("class name is pascal-cased", "component_class = star::demo_mod::DemoNode" in conf_text)

    header_text = (component_dir / "demo_node_component.h").read_text()
    check("registration string matches conf class", 'registry.register_component<DemoNodeComponent>("star::demo_mod::DemoNode")' in header_text)
    check("plugin entry is fixed signature", "extern \"C\" void star_register_components" in (component_dir / "register.cc").read_text())

    check("parent module.conf lists component", "components [demo_node/component.conf]" in module_conf.read_text())
    check("parent meson includes component", "subdir('demo_node')" in (root / "modules" / "demo_mod" / "meson.build").read_text())

    # 错误路径：不覆盖已有内容、模块必须先存在、名字必须合法
    check("duplicate component is rejected", run(root, "component", "demo_mod/demo_node").returncode != 0)
    check("missing module is rejected", run(root, "component", "nope/x").returncode != 0)
    check("invalid module name is rejected", run(root, "module", "Demo").returncode != 0)

    # 第二个组件应追加到 components 列表而不是覆盖
    result = run(root, "component", "demo_mod/other_node")
    check("second component appends to module.conf", "demo_node/component.conf, other_node/component.conf" in module_conf.read_text())

if _failures:
    print(f"\nscaffold selftest: {len(_failures)} failure(s)")
    raise SystemExit(1)
print("\nscaffold selftest: all checks passed")
