from __future__ import annotations

import json
from dataclasses import dataclass, field

_PRIMITIVE_TYPES = {
    "string",
    "bool",
    "int",
    "int32",
    "int64",
    "uint32",
    "uint64",
    "float",
    "double",
    "float64",
    "object",
}



@dataclass
class TypeRef:
    category: str
    name: str = ""
    key_name: str = ""
    value_name: str = ""
    optional: bool = False
    explicit_required: bool = False


@dataclass
class FieldDef:
    id: int
    name: str
    type_ref: TypeRef
    default: str | None = None


@dataclass
class DefBlock:
    name: str
    fields: list[FieldDef] = field(default_factory=list)


@dataclass
class SchemaDoc:
    schema_name: str = ""
    root_name: str = ""
    # 一个 .schema 可以有多个 root（多个消息类型）；roots[0] 兼作默认 root。
    # 类型身份：单 root 用 schema 名，多 root 用 "schema.root"，保证每类型身份唯一。
    roots: list[str] = field(default_factory=list)
    namespace_name: str = ""
    version: int = 0
    hash_value: int = 0
    allow_unknown: bool = False
    defs: list[DefBlock] = field(default_factory=list)




def _fnv1a64(text: str) -> int:
    """确定性 64 位内容哈希（FNV-1a）。

    必须跨平台、跨 Python 版本稳定：不能用内置 hash()（带随机盐）。
    SchemaTypeId.hash 由它填充，跨 DSO、跨进程、跨语言都要能独立算出同一个值。
    """
    value = 0xCBF29CE484222325
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def _reachable_defs(doc: "SchemaDoc", root_name: str) -> list["DefBlock"]:
    """root 可达的 def 闭包（按名字排序）。

    哈希只覆盖该 root 真正用到的定义：多 root 文件里改 B 不应让 A 的身份失效。
    """
    by_name = {block.name: block for block in doc.defs}
    seen: set[str] = set()
    stack = [root_name]
    while stack:
        name = stack.pop()
        if name in seen or name not in by_name:
            continue
        seen.add(name)
        for field_def in by_name[name].fields:
            if field_def.type_ref.category == "map":
                refs = [field_def.type_ref.value_name]
            else:
                refs = [field_def.type_ref.name]
            stack.extend(ref for ref in refs if ref in by_name)
    return [by_name[name] for name in sorted(seen)]


def schema_content_hash(doc: "SchemaDoc", root_name: str | None = None) -> int:
    """对 schema 的语义内容算哈希（按 root）。

    只覆盖契约本身：类型名/版本/命名空间/root/该 root 可达的字段定义（按 id 排序）。
    字段在源文件里的书写顺序、注释、空行、fmt 重排都不影响结果。
    """
    root = root_name or doc.root_name
    payload = {
        "schema": doc.schema_name,
        "version": doc.version,
        "namespace": doc.namespace_name,
        "root": root,
        "allow_unknown": doc.allow_unknown,
        "defs": [
            {
                "name": block.name,
                "fields": [
                    {
                        "id": item.id,
                        "name": item.name,
                        "category": item.type_ref.category,
                        "type": item.type_ref.name,
                        "key": item.type_ref.key_name,
                        "value": item.type_ref.value_name,
                        "optional": item.type_ref.optional,
                        "required": item.type_ref.explicit_required,
                        "default": item.default,
                    }
                    for item in sorted(block.fields, key=lambda value: (value.id, value.name))
                ],
            }
            for block in _reachable_defs(doc, root)
        ],
    }
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return _fnv1a64(canonical)
