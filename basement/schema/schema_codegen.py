#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import sys

from codegen.cpp import header_text
from codegen.ir import DefBlock, FieldDef, SchemaDoc, TypeRef, _PRIMITIVE_TYPES, schema_content_hash
from codegen.loader import load_schema
from codegen.parser import Parser
from codegen.text import format_schema


def run_gen(schema: pathlib.Path, output: pathlib.Path, include_root: pathlib.Path | None = None) -> int:
    try:
        doc = load_schema(schema)
    except ValueError as exc:
        print(f"{schema}: {exc}", file=sys.stderr)
        return 2
    issues = collect_issues(doc)
    if issues:
        for issue in issues:
            print(f"{schema}: {issue}", file=sys.stderr)
        return 2
    try:
        header = header_text(doc, output, include_root)
    except ValueError as exc:
        print(f"{schema}: {exc}", file=sys.stderr)
        return 2
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(header, encoding="utf-8")
    return 0


def run_gen_dir(schema_dir: pathlib.Path, output_dir: pathlib.Path, include_root: pathlib.Path | None = None) -> int:
    for schema_path in sorted(schema_dir.rglob("*.schema")):
        rel = schema_path.relative_to(schema_dir)
        output_path = output_dir / rel.with_suffix(".s.h")
        ret = run_gen(schema_path, output_path, include_root)
        if ret != 0:
            return ret
    return 0


def run_fmt(schema: pathlib.Path, output: pathlib.Path | None, in_place: bool) -> int:
    doc = Parser(schema.read_text(encoding="utf-8")).parse()
    formatted = format_schema(doc)
    if in_place:
        schema.write_text(formatted, encoding="utf-8")
        return 0
    if output is not None:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(formatted, encoding="utf-8")
        return 0
    print(formatted, end="")
    return 0


def type_ref_key(type_ref: TypeRef) -> tuple[str, str, str, bool, bool]:
    return (
        type_ref.category,
        type_ref.name,
        type_ref.key_name,
        type_ref.optional,
        type_ref.explicit_required,
    )


def compat_type_allowed(old_type: TypeRef, new_type: TypeRef) -> bool:
    old_key = type_ref_key(old_type)
    new_key = type_ref_key(new_type)
    if old_key == new_key and old_type.value_name == new_type.value_name:
        return True

    if old_type.category != new_type.category:
        return False
    if old_type.optional != new_type.optional:
        return False
    if old_type.explicit_required != new_type.explicit_required:
        return False

    widening = {
        "uint32": {"uint64"},
        "uint64": set(),
        "int32": {"int64"},
        "int64": set(),
        "int": {"int64"},
    }
    if old_type.category == "simple":
        if old_type.name == new_type.name:
            return True
        return new_type.name in widening.get(old_type.name, set())
    if old_type.category == "array":
        if old_type.name == new_type.name:
            return True
        return new_type.name in widening.get(old_type.name, set())
    if old_type.category == "map":
        if old_type.key_name != new_type.key_name:
            return False
        if old_type.value_name == new_type.value_name:
            return True
        return new_type.value_name in widening.get(old_type.value_name, set())
    return False


def field_type_display(type_ref: TypeRef) -> str:
    return format_type_ref(type_ref)


def compare_blocks(
    old_doc: SchemaDoc,
    new_doc: SchemaDoc,
    old_block: DefBlock,
    new_block: DefBlock,
    old_defs: dict[str, DefBlock],
    new_defs: dict[str, DefBlock],
    issues: list[str],
    path: str,
    visited: set[tuple[str, str]],
) -> None:
    visit_key = (old_block.name, new_block.name)
    if visit_key in visited:
        return
    visited.add(visit_key)

    old_by_id = {field.id: field for field in old_block.fields}
    new_by_id = {field.id: field for field in new_block.fields}
    old_name_to_id = {field.name: field.id for field in old_block.fields}
    new_name_to_id = {field.name: field.id for field in new_block.fields}

    for old_id, old_field in old_by_id.items():
        if old_id not in new_by_id:
            if old_field.name in new_name_to_id and new_name_to_id[old_field.name] != old_id:
                issues.append(
                    f"{path}: field '{old_field.name}' changed id {old_id} -> {new_name_to_id[old_field.name]}"
                )
            else:
                issues.append(f"{path}: field id {old_id} removed (old name='{old_field.name}')")
            continue

        new_field = new_by_id[old_id]
        if not compat_type_allowed(old_field.type_ref, new_field.type_ref):
            issues.append(
                f"{path}: field id {old_id} type incompatible "
                f"{field_type_display(old_field.type_ref)} -> {field_type_display(new_field.type_ref)}"
            )
            continue

        old_target = old_field.type_ref.value_name if old_field.type_ref.category == "map" else old_field.type_ref.name
        new_target = new_field.type_ref.value_name if new_field.type_ref.category == "map" else new_field.type_ref.name
        if old_target in old_defs and new_target in new_defs:
            compare_blocks(
                old_doc,
                new_doc,
                old_defs[old_target],
                new_defs[new_target],
                old_defs,
                new_defs,
                issues,
                f"{path}.{new_field.name}",
                visited,
            )

def run_compat(old_schema: pathlib.Path, new_schema: pathlib.Path) -> int:
    try:
        old_doc = Parser(old_schema.read_text(encoding="utf-8")).parse()
        new_doc = Parser(new_schema.read_text(encoding="utf-8")).parse()
    except Exception as exc:
        print(f"compat parse error: {exc}", file=sys.stderr)
        return 2

    issues: list[str] = []
    if old_doc.schema_name != new_doc.schema_name:
        issues.append(f"schema name changed: {old_doc.schema_name} -> {new_doc.schema_name}")
    if old_doc.root_name != new_doc.root_name:
        issues.append(f"root name changed: {old_doc.root_name} -> {new_doc.root_name}")

    old_defs = {block.name: block for block in old_doc.defs}
    new_defs = {block.name: block for block in new_doc.defs}
    if old_doc.root_name not in old_defs or new_doc.root_name not in new_defs:
        print("compat parse error: root def not found", file=sys.stderr)
        return 2

    compare_blocks(
        old_doc,
        new_doc,
        old_defs[old_doc.root_name],
        new_defs[new_doc.root_name],
        old_defs,
        new_defs,
        issues,
        old_doc.root_name,
        set(),
    )

    if issues:
        for issue in issues:
            print(f"BREAKING: {issue}")
        return 1
    print("COMPATIBLE")
    return 0


def collect_issues(doc: SchemaDoc) -> list[str]:
    """schema 的结构与契约检查。gen 与 lint 共用同一套规则，
    避免「能生成但 lint 不过」或反过来的分裂。"""
    issues: list[str] = []
    defs = {block.name: block for block in doc.defs}
    external_names: set[str] = set()
    for imported in doc.imported_docs:
        for block in imported.defs:
            if block.name in defs:
                issues.append(f'import name collision: "{block.name}" is defined both locally and in {imported.source_path}')
            elif block.name in external_names:
                issues.append(f'import name collision: "{block.name}" is provided by multiple imports')
            external_names.add(block.name)
    if not doc.roots:
        issues.append("schema has no root block")
    for root_name in doc.roots:
        if root_name not in defs:
            issues.append(f"root '{root_name}' not found in root/def blocks")

    if doc.version < 1:
        issues.append(f"version must be >= 1 (declared {doc.version}); every schema is a versioned contract")

    computed = schema_content_hash(doc)
    if doc.hash_value and doc.hash_value != computed:
        issues.append(
            f"declared hash {doc.hash_value} does not match content hash {computed}; "
            f"run `star schema hash --schema=<file>` and update the `hash` line"
        )

    for block in doc.defs:
        seen_ids: dict[int, str] = {}
        seen_names: set[str] = set()
        for field in block.fields:
            if field.id <= 0:
                issues.append(f"{block.name}: field '{field.name}' has invalid id {field.id}")
            elif field.id in seen_ids:
                issues.append(
                    f"{block.name}: duplicate field id {field.id} for '{seen_ids[field.id]}' and '{field.name}'"
                )
            else:
                seen_ids[field.id] = field.name

            if field.name in seen_names:
                issues.append(f"{block.name}: duplicate field name '{field.name}'")
            else:
                seen_names.add(field.name)

            if field.type_ref.category == "map":
                if field.type_ref.key_name != "string":
                    issues.append(
                        f"{block.name}: map field '{field.name}' key type must be 'string' "
                        f"(codegen only supports std::string keys), got '{field.type_ref.key_name}'"
                    )
                target = field.type_ref.value_name
            else:
                target = field.type_ref.name
            if target not in _PRIMITIVE_TYPES and target not in defs and target not in external_names:
                issues.append(f"{block.name}: field '{field.name}' references undefined type '{target}'")

    return issues


def run_lint(schema: pathlib.Path) -> int:
    try:
        # 用加载器而不是裸 Parser：import 的符号要参与「未定义类型」检查
        doc = load_schema(schema)
    except ValueError as exc:
        print(f"lint import error: {exc}", file=sys.stderr)
        return 2
    except Exception as exc:
        print(f"lint parse error: {exc}", file=sys.stderr)
        return 2

    issues = collect_issues(doc)
    if issues:
        for issue in issues:
            print(f"LINT: {issue}")
        return 1
    print("LINT_OK")
    return 0


def run_hash(schema: pathlib.Path) -> int:
    try:
        doc = Parser(schema.read_text(encoding="utf-8")).parse()
    except Exception as exc:
        print(f"hash parse error: {exc}", file=sys.stderr)
        return 2
    print(schema_content_hash(doc))
    return 0


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] not in {"gen", "fmt", "compat", "lint", "hash"}:
        parser = argparse.ArgumentParser()
        parser.add_argument("--schema", required=True)
        parser.add_argument("--output", required=True)
        args = parser.parse_args()
        include_root = pathlib.Path(args.include_root) if args.include_root else None
        return run_gen(pathlib.Path(args.schema), pathlib.Path(args.output), include_root)

    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    gen_parser = subparsers.add_parser("gen")
    gen_parser.add_argument("--schema", default=None)
    gen_parser.add_argument("--output", default=None)
    gen_parser.add_argument("--schema-dir", default=None)
    gen_parser.add_argument("--output-dir", default=None)
    gen_parser.add_argument("--include-root", default=None)

    fmt_parser = subparsers.add_parser("fmt")
    fmt_parser.add_argument("--schema", required=True)
    fmt_parser.add_argument("--output")
    fmt_parser.add_argument("--in-place", action="store_true")

    compat_parser = subparsers.add_parser("compat")
    compat_parser.add_argument("old_schema")
    compat_parser.add_argument("new_schema")

    lint_parser = subparsers.add_parser("lint")
    lint_parser.add_argument("--schema", required=True)

    hash_parser = subparsers.add_parser("hash")
    hash_parser.add_argument("--schema", required=True)

    args = parser.parse_args()
    if args.command == "gen":
        if args.schema_dir:
            include_root = pathlib.Path(args.include_root) if args.include_root else None
            return run_gen_dir(pathlib.Path(args.schema_dir), pathlib.Path(args.output_dir), include_root)
        include_root = pathlib.Path(args.include_root) if args.include_root else None
        return run_gen(pathlib.Path(args.schema), pathlib.Path(args.output), include_root)
    if args.command == "fmt":
        return run_fmt(pathlib.Path(args.schema), pathlib.Path(args.output) if args.output else None, args.in_place)
    if args.command == "compat":
        return run_compat(pathlib.Path(args.old_schema), pathlib.Path(args.new_schema))
    if args.command == "lint":
        return run_lint(pathlib.Path(args.schema))
    if args.command == "hash":
        return run_hash(pathlib.Path(args.schema))
    raise ValueError(f"unknown command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
