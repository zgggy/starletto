#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import sys
from dataclasses import dataclass

import schema_codegen


@dataclass
class CompatIssue:
    level: str
    path: str
    message: str


def type_repr(type_ref: schema_codegen.TypeRef) -> str:
    if type_ref.category == "map":
        base = f"{{{type_ref.key_name} : {type_ref.value_name}}}"
    elif type_ref.category == "array":
        base = f"{type_ref.name}[]"
    else:
        base = type_ref.name
    if type_ref.optional:
        return f"{base} ?"
    if type_ref.explicit_required:
        return f"{base} !"
    return base


def fields_by_id(block: schema_codegen.DefBlock) -> dict[int, schema_codegen.FieldDef]:
    out: dict[int, schema_codegen.FieldDef] = {}
    for field in block.fields:
        if field.id <= 0:
            raise ValueError(f"missing field id in {block.name}.{field.name}")
        if field.id in out:
            raise ValueError(f"duplicate field id {field.id} in {block.name}")
        out[field.id] = field
    return out


def defs_by_name(doc: schema_codegen.SchemaDoc) -> dict[str, schema_codegen.DefBlock]:
    out: dict[str, schema_codegen.DefBlock] = {}
    for item in doc.defs:
        if item.name in out:
            raise ValueError(f"duplicate def {item.name}")
        out[item.name] = item
    return out


def compare_type_ref(path: str, old: schema_codegen.TypeRef, new: schema_codegen.TypeRef, issues: list[CompatIssue]) -> None:
    if schema_codegen.compat_type_allowed(old, new):
        return
    issues.append(CompatIssue("breaking", path, f"type incompatible: {type_repr(old)} -> {type_repr(new)}"))


def compare_block(
    block_name: str,
    old: schema_codegen.DefBlock,
    new: schema_codegen.DefBlock,
    all_old_defs: dict[str, schema_codegen.DefBlock],
    all_new_defs: dict[str, schema_codegen.DefBlock],
    issues: list[CompatIssue],
    visited: set[str],
) -> None:
    visit_key = f"{block_name}"
    if visit_key in visited:
        return
    visited.add(visit_key)

    old_fields = fields_by_id(old)
    new_fields = fields_by_id(new)

    for field_id, old_field in old_fields.items():
        path = f"{block_name}.{old_field.name}"
        new_field = new_fields.get(field_id)
        if new_field is None:
            issues.append(CompatIssue("breaking", path, f"field removed: id={field_id}, name={old_field.name}"))
            continue
        compare_type_ref(path, old_field.type_ref, new_field.type_ref, issues)
        if old_field.name != new_field.name:
            issues.append(CompatIssue("info", path, f"field renamed: {old_field.name} -> {new_field.name}"))
        if old_field.default != new_field.default:
            issues.append(CompatIssue("info", path, "default value changed"))

        old_ref = referenced_def_name(old_field.type_ref, all_old_defs)
        new_ref = referenced_def_name(new_field.type_ref, all_new_defs)
        if old_ref and new_ref:
            compare_block(old_ref, all_old_defs[old_ref], all_new_defs[new_ref], all_old_defs, all_new_defs, issues, visited)
        elif old_ref != new_ref:
            issues.append(CompatIssue("breaking", path, f"referenced type changed: {old_ref} -> {new_ref}"))

    for field_id, new_field in new_fields.items():
        if field_id not in old_fields:
            issues.append(CompatIssue("info", f"{block_name}.{new_field.name}", f"field added: id={field_id}, name={new_field.name}"))


def referenced_def_name(type_ref: schema_codegen.TypeRef, defs: dict[str, schema_codegen.DefBlock]) -> str | None:
    if type_ref.category == "simple":
        return type_ref.name if type_ref.name in defs else None
    if type_ref.category == "array":
        return type_ref.name if type_ref.name in defs else None
    if type_ref.category == "map":
        return type_ref.value_name if type_ref.value_name in defs else None
    return None


def parse_schema(path: pathlib.Path) -> schema_codegen.SchemaDoc:
    return schema_codegen.Parser(path.read_text(encoding="utf-8")).parse()


def run_compat(old_path: pathlib.Path, new_path: pathlib.Path) -> int:
    try:
        old_doc = parse_schema(old_path)
        new_doc = parse_schema(new_path)
        old_defs = defs_by_name(old_doc)
        new_defs = defs_by_name(new_doc)
    except Exception as e:
        print(f"[E] schema_compat parse failed: {e}", file=sys.stderr)
        return 2

    if old_doc.root_name not in old_defs:
        print(f"[E] old schema missing root def: {old_doc.root_name}", file=sys.stderr)
        return 2
    if new_doc.root_name not in new_defs:
        print(f"[E] new schema missing root def: {new_doc.root_name}", file=sys.stderr)
        return 2

    issues: list[CompatIssue] = []
    visited: set[str] = set()
    compare_block(old_doc.root_name, old_defs[old_doc.root_name], new_defs[new_doc.root_name], old_defs, new_defs, issues, visited)

    breaking = [item for item in issues if item.level == "breaking"]
    infos = [item for item in issues if item.level != "breaking"]

    for item in breaking:
        print(f"[BREAKING] {item.path}: {item.message}")
    for item in infos:
        print(f"[INFO] {item.path}: {item.message}")

    return 1 if breaking else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("old_schema")
    parser.add_argument("new_schema")
    args = parser.parse_args()
    return run_compat(pathlib.Path(args.old_schema), pathlib.Path(args.new_schema))


if __name__ == "__main__":
    raise SystemExit(main())
