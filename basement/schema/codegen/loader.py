from __future__ import annotations

import pathlib

from .ir import SchemaDoc
from .parser import Parser


def load_schema(path: pathlib.Path, stack: list[pathlib.Path] | None = None) -> SchemaDoc:
    """解析一个 .schema 并递归加载它的直接 import。

    只做一层可见性：A import B 后，A 能引用 B 的类型；B 再 import C，C 的类型
    对 A 不可见（B 的生成头会 include C，但 A 需要自己 import C 才能引用）。
    这样符号表和 include 关系一一对应，不会出现「能编译但依赖运气」的传递引用。
    """
    resolved = path.resolve()
    stack = list(stack or [])
    if resolved in stack:
        chain = " -> ".join(str(item) for item in stack + [resolved])
        raise ValueError(f"cyclic schema import: {chain}")
    if not resolved.exists():
        raise ValueError(f"schema not found: {resolved}")

    doc = Parser(resolved.read_text(encoding="utf-8")).parse()
    doc.source_path = str(resolved)

    seen: set[pathlib.Path] = set()
    for relative in doc.imports:
        target = (resolved.parent / relative).resolve()
        if target in seen:
            continue
        seen.add(target)
        if not target.exists():
            raise ValueError(f"import not found: \"{relative}\" (from {resolved})")
        doc.imported_docs.append(load_schema(target, stack + [resolved]))
    return doc
