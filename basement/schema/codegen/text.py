from __future__ import annotations

from .ir import DefBlock, FieldDef, SchemaDoc, TypeRef


def format_type_ref(type_ref: TypeRef) -> str:
    if type_ref.category == "array":
        base = f"{type_ref.name}[]"
    elif type_ref.category == "map":
        value_ref = TypeRef(category="simple", name=type_ref.value_name)
        base = f"{{string : {format_type_ref(value_ref)}}}"
    else:
        base = type_ref.name
    if type_ref.optional:
        return f"{base} ?"
    if type_ref.explicit_required:
        return f"{base} !"
    return base


def format_field(field: FieldDef) -> str:
    default = f" = {field.default}" if field.default is not None else ""
    return f"    {field.id} {field.name} : {format_type_ref(field.type_ref)}{default}"


def format_block(keyword: str, block: DefBlock) -> list[str]:
    lines = [f"{keyword} {block.name} {{"]
    for field in block.fields:
        lines.append(format_field(field))
    if block.reserved_ids:
        lines.append("    reserved " + ", ".join(str(value) for value in sorted(block.reserved_ids)) + ";")
    lines.append("}")
    return lines


def format_schema(doc: SchemaDoc) -> str:
    lines: list[str] = [f"schema {doc.schema_name}"]
    for imported in doc.imports:
        lines.append(f'import "{imported}"')
    root_set = set(doc.roots)
    by_name = {block.name: block for block in doc.defs}
    emitted_roots = 0
    for root_name in doc.roots:
        root_block = by_name.get(root_name)
        if root_block is None:
            lines.append(f"root {root_name}")
        else:
            lines.append("")
            lines.extend(format_block("root", root_block))
        emitted_roots += 1
    if emitted_roots == 0:
        lines.append(f"root {doc.root_name}")
    if doc.namespace_name:
        lines.append("")
        lines.append(f"namespace {doc.namespace_name}")
    if doc.version:
        lines.append(f"version {doc.version}")
    if doc.hash_value:
        lines.append(f"hash {doc.hash_value}")
    if doc.allow_unknown:
        lines.append("allow_unknown true")

    other_defs = [block for block in doc.defs if block.name not in root_set]
    if other_defs:
        lines.append("")
        for index, block in enumerate(other_defs):
            if index > 0:
                lines.append("")
            lines.extend(format_block("def", block))
    lines.append("")
    return "\n".join(lines)


