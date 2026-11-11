#!/usr/bin/env python3
"""schema codegen 行为自检（不依赖构建产物，由 `star check` 调用）。

覆盖那些 C++ 单测覆盖不到、又必须在 CI 里锁住的纯 Python 行为：
reserved 语义、哈希稳定性、多 root 身份、import 的循环/缺失处理、未定义类型。
"""

from __future__ import annotations

import pathlib
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "basement" / "schema"))

from codegen.cpp import header_text  # noqa: E402
from codegen.ir import schema_content_hash  # noqa: E402
from codegen.loader import load_schema  # noqa: E402
from codegen.parser import Parser  # noqa: E402
from codegen.text import format_schema  # noqa: E402
import schema_codegen  # noqa: E402

_failures: list[str] = []


def check(name: str, condition: bool, detail: str = "") -> None:
    if condition:
        print(f"[ OK ] {name}")
        return
    print(f"[FAIL] {name}" + (f": {detail}" if detail else ""))
    _failures.append(name)


def parse(text: str):
    return Parser(text).parse()


# reserved：fmt 往返、与字段冲突必须被 lint 拦下、参与内容哈希
doc = parse("schema t.x\nversion 1\nroot R {\n    1 a : int\n    reserved 2, 4;\n}\n")
formatted = format_schema(doc)
doc_roundtrip = parse(formatted)
check("reserved survives fmt roundtrip", doc_roundtrip.defs[0].reserved_ids == [2, 4], formatted)

conflict = parse("schema t.x\nversion 1\nroot R {\n    1 a : int\n    reserved 1;\n}\n")
issues = schema_codegen.collect_issues(conflict)
check("reserved id colliding with a field is rejected", any("reserved id 1 collides" in issue for issue in issues), str(issues))

same_payload = parse("schema t.h\nversion 1\nroot R {\n    1 a : int\n    2 b : string\n}\n")
reordered = parse("schema t.h\nversion 1\nroot R {\n    2 b : string\n    1 a : int\n}\n")
check("field source order does not change the hash", schema_content_hash(same_payload) == schema_content_hash(reordered))

with_reserved = parse("schema t.h\nversion 1\nroot R {\n    1 a : int\n    2 b : string\n    reserved 3;\n}\n")
check("reserved set participates in the hash", schema_content_hash(same_payload) != schema_content_hash(with_reserved))

# 多 root：身份名与哈希都按 root 独立
multi = parse("schema t.m\nversion 1\nnamespace star.test.m\nroot A {\n    1 a : int\n}\nroot B {\n    1 b : int\n}\n")
header = header_text(multi, pathlib.Path("/tmp/unused.s.h"))
check("multi-root identity names are distinct", '"t.m.A"' in header and '"t.m.B"' in header)
check("multi-root hashes differ per root", schema_content_hash(multi, "A") != schema_content_hash(multi, "B"))

# 未定义类型
undefined = parse("schema t.u\nversion 1\nroot R {\n    1 a : NoSuchType\n}\n")
check("undefined type is rejected", any("undefined type" in issue for issue in schema_codegen.collect_issues(undefined)))

# import：循环与缺失必须给出明确错误
with tempfile.TemporaryDirectory() as tmp:
    base = pathlib.Path(tmp)
    (base / "a.schema").write_text('schema t.a\nversion 1\nimport "b.schema"\nroot A {\n    1 a : int\n}\n', encoding="utf-8")
    (base / "b.schema").write_text('schema t.b\nversion 1\nimport "a.schema"\nroot B {\n    1 b : int\n}\n', encoding="utf-8")
    try:
        load_schema(base / "a.schema")
        cycle_ok = False
    except ValueError as exc:
        cycle_ok = "cyclic" in str(exc)
    check("cyclic import is rejected", cycle_ok)

    (base / "c.schema").write_text('schema t.c\nversion 1\nimport "missing.schema"\nroot C {\n    1 c : int\n}\n', encoding="utf-8")
    try:
        load_schema(base / "c.schema")
        missing_ok = False
    except ValueError as exc:
        missing_ok = "not found" in str(exc)
    check("missing import is rejected", missing_ok)

if _failures:
    print(f"\ncodegen selftest: {len(_failures)} failure(s)")
    raise SystemExit(1)
print("\ncodegen selftest: all checks passed")
