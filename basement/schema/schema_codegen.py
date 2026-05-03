#!/usr/bin/env python3

from __future__ import annotations

import argparse

_BINARY_ONLY_TYPES = {"SchemaValue"}

_SCHEMA_TEXT_TYPES = {"object"}

def _is_binary_only_type(type_name):
    return type_name in _BINARY_ONLY_TYPES

def _is_schema_text_type(type_name):
    return type_name in _SCHEMA_TEXT_TYPES

def _cpp_type_for_binary_only(type_name):
    if type_name == "object":
        return "star::schema::SchemaValue"
    return type_name
import pathlib
import re
import sys
from dataclasses import dataclass, field


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
    namespace_name: str = ""
    version: int = 0
    hash_value: int = 0
    allow_unknown: bool = False
    defs: list[DefBlock] = field(default_factory=list)


class Parser:
    def __init__(self, text: str) -> None:
        self.text = text
        self.pos = 0

    def eof(self) -> bool:
        return self.pos >= len(self.text)

    def peek(self) -> str:
        if self.eof():
            return "\0"
        return self.text[self.pos]

    def get(self) -> str:
        if self.eof():
            return "\0"
        ch = self.text[self.pos]
        self.pos += 1
        return ch

    def expect(self, ch: str) -> None:
        if self.peek() != ch:
            raise ValueError(f"expect '{ch}' at position {self.pos}")
        self.pos += 1

    def skip_ws_and_comments(self) -> None:
        while not self.eof():
            ch = self.peek()
            if ch.isspace():
                self.pos += 1
                continue
            if ch == "#":
                while not self.eof() and self.peek() != "\n":
                    self.pos += 1
                continue
            if ch == "/" and self.pos + 1 < len(self.text) and self.text[self.pos + 1] == "/":
                self.pos += 2
                while not self.eof() and self.peek() != "\n":
                    self.pos += 1
                continue
            break

    def parse_identifier(self) -> str:
        self.skip_ws_and_comments()
        if self.peek() == '"':
            return self.parse_string()
        out = []
        while not self.eof():
            ch = self.peek()
            if ch.isalnum() or ch in "_-.:/":
                out.append(self.get())
            else:
                break
        if not out:
            raise ValueError(f"empty identifier at position {self.pos}")
        return "".join(out)

    def parse_string(self) -> str:
        self.expect('"')
        out = []
        while not self.eof():
            ch = self.get()
            if ch == '"':
                return "".join(out)
            if ch == "\\":
                esc = self.get()
                mapping = {"n": "\n", "r": "\r", "t": "\t", '"': '"', "\\": "\\"}
                out.append(mapping.get(esc, esc))
                continue
            out.append(ch)
        raise ValueError("unterminated string")

    def parse_integer(self) -> int:
        self.skip_ws_and_comments()
        out = []
        while not self.eof() and self.peek().isdigit():
            out.append(self.get())
        if not out:
            raise ValueError(f"empty integer at position {self.pos}")
        return int("".join(out))

    def parse_type_ref(self) -> TypeRef:
        self.skip_ws_and_comments()
        if self.peek() == "[":
            self.expect("[")
            name = self.parse_identifier()
            self.skip_ws_and_comments()
            self.expect("]")
            out = TypeRef(category="array", name=name)
        elif self.peek() == "{":
            self.expect("{")
            key_name = self.parse_identifier()
            self.skip_ws_and_comments()
            self.expect(":")
            value_name = self.parse_identifier()
            self.skip_ws_and_comments()
            self.expect("}")
            out = TypeRef(category="map", key_name=key_name, value_name=value_name)
        else:
            out = TypeRef(category="simple", name=self.parse_identifier())
        self.skip_ws_and_comments()
        if self.peek() == "[":
            self.expect("[")
            self.expect("]")
            out.category = "array"
        self.skip_ws_and_comments()
        if self.peek() == "?":
            self.expect("?")
            out.optional = True
        elif self.peek() == "!":
            self.expect("!")
            out.explicit_required = True
        return out

    def parse_default(self) -> str:
        self.skip_ws_and_comments()
        if self.peek() == '"':
            return '"' + self.parse_string().replace("\\", "\\\\").replace('"', '\\"') + '"'
        out = []
        while not self.eof():
            ch = self.peek()
            if ch.isspace() or ch == "}":
                break
            out.append(self.get())
        if not out:
            raise ValueError("empty default value")
        return "".join(out)

    def parse_field(self) -> FieldDef:
        self.skip_ws_and_comments()
        field_id = 0
        if self.peek().isdigit():
            field_id = int(self.parse_identifier())
            self.skip_ws_and_comments()
        name = self.parse_identifier()
        self.skip_ws_and_comments()
        self.expect(":")
        type_ref = self.parse_type_ref()
        self.skip_ws_and_comments()
        default = None
        if self.peek() == "=":
            self.expect("=")
            default = self.parse_default()
        return FieldDef(id=field_id, name=name, type_ref=type_ref, default=default)

    def parse_def(self) -> DefBlock:
        name = self.parse_identifier()
        self.skip_ws_and_comments()
        self.expect("{")
        out = DefBlock(name=name)
        while True:
            self.skip_ws_and_comments()
            if self.peek() == "}":
                self.expect("}")
                return out
            out.fields.append(self.parse_field())

    def parse(self) -> SchemaDoc:
        doc = SchemaDoc()
        while True:
            self.skip_ws_and_comments()
            if self.eof():
                break
            keyword = self.parse_identifier()
            if keyword == "schema":
                doc.schema_name = self.parse_identifier()
                continue
            if keyword == "root":
                root_name = self.parse_identifier()
                self.skip_ws_and_comments()
                if self.peek() == "{":
                    self.expect("{")
                    root = DefBlock(name=root_name)
                    while True:
                        self.skip_ws_and_comments()
                        if self.peek() == "}":
                            self.expect("}")
                            break
                        root.fields.append(self.parse_field())
                    doc.root_name = root.name
                    doc.defs.append(root)
                else:
                    doc.root_name = root_name
                continue
            if keyword == "namespace":
                doc.namespace_name = self.parse_identifier()
                continue
            if keyword == "version":
                doc.version = self.parse_integer()
                continue
            if keyword == "hash":
                doc.hash_value = self.parse_integer()
                continue
            if keyword == "allow_unknown":
                value = self.parse_identifier()
                doc.allow_unknown = value in {"true", "T", "1"}
                continue
            if keyword == "def":
                doc.defs.append(self.parse_def())
                continue
            raise ValueError(f"unknown keyword: {keyword}")
        if not doc.schema_name:
            raise ValueError("schema name is empty")
        if not doc.root_name:
            doc.root_name = doc.schema_name.split(".")[-1]
        return doc


def cpp_namespace(doc: SchemaDoc) -> list[str]:
    raw = doc.namespace_name or f"star.generated.{doc.schema_name.replace('-', '_')}"
    return [part for part in re.split(r"[:.]+", raw) if part]


def guard_name(schema_name: str) -> str:
    text = schema_name.upper().replace(".", "_").replace("-", "_").replace("/", "_")
    return f"__{text}__"


def cpp_type(type_ref: TypeRef, has_default: bool) -> str:
    if type_ref.category == "array":
        inner = cpp_type(TypeRef(category="simple", name=type_ref.name), False)
        base = f"std::vector<{inner}>"
    elif type_ref.category == "map":
        inner = cpp_type(TypeRef(category="simple", name=type_ref.value_name), False)
        base = f"std::map<std::string, {inner}>"
    else:
        mapping = {
            "string": "std::string",
            "bool": "bool",
            "int": "int64_t",
            "int32": "int64_t",
            "int64": "int64_t",
            "uint32": "int64_t",
            "uint64": "int64_t",
            "float": "double",
            "double": "double",
            "float64": "double",
            "object": "star::schema::SchemaObject",
        }
        base = mapping.get(type_ref.name, type_ref.name)
    if type_ref.optional and not has_default:
        return f"std::optional<{base}>"
    return base


def default_expr(field: FieldDef) -> str:
    if field.default is None:
        if field.type_ref.optional:
            return "std::nullopt"
        return ""
    if field.type_ref.name == "string":
        return field.default
    if field.type_ref.name == "bool":
        return "true" if field.default == "true" else "false"
    if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64"}:
        return f"static_cast<int64_t>({field.default})"
    if field.type_ref.name in {"float", "double", "float64"}:
        return f"static_cast<double>({field.default})"
    return "{}"


def descriptor_default_expr(field: FieldDef) -> str:
    if field.default is None:
        return ""
    if field.type_ref.name == "string":
        return f"star::schema::SchemaValue({field.default})"
    if field.type_ref.name == "bool":
        v = "true" if field.default == "true" else "false"
        return f"star::schema::SchemaValue({v})"
    if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64"}:
        return f"star::schema::SchemaValue(static_cast<int64_t>({field.default}))"
    if field.type_ref.name in {"float", "double", "float64"}:
        return f"star::schema::SchemaValue(static_cast<double>({field.default}))"
    return "{}"


def read_expr(field: FieldDef) -> str:
    base = cpp_type(TypeRef(category="simple", name=field.type_ref.name or field.type_ref.value_name), False)
    if field.type_ref.category == "array":
        inner = cpp_type(TypeRef(category="simple", name=field.type_ref.name), False)
        return f"schema_read<std::vector<{inner}>>(it->second)"
    if field.type_ref.category == "map":
        inner = cpp_type(TypeRef(category="simple", name=field.type_ref.value_name), False)
        return f"schema_read<std::map<std::string, {inner}>>(it->second)"
    return f"schema_read<{base}>(it->second)"


def field_kind_expr(type_ref: TypeRef, defs: dict[str, DefBlock]) -> str:
    target = type_ref.name
    if type_ref.category == "map":
        target = type_ref.value_name
    mapping = {
        "bool": "star::schema::FieldKind::BOOL",
        "int": "star::schema::FieldKind::INT64",
        "int32": "star::schema::FieldKind::INT64",
        "int64": "star::schema::FieldKind::INT64",
        "uint32": "star::schema::FieldKind::INT64",
        "uint64": "star::schema::FieldKind::INT64",
        "float": "star::schema::FieldKind::FLOAT64",
        "double": "star::schema::FieldKind::FLOAT64",
        "float64": "star::schema::FieldKind::FLOAT64",
        "string": "star::schema::FieldKind::STRING",
    }
    if target == "object" or target in defs:
        return "star::schema::FieldKind::OBJECT"
    return mapping.get(target, "star::schema::FieldKind::STRING")


def field_type_name(type_ref: TypeRef) -> str:
    if type_ref.category == "map":
        return type_ref.value_name
    return type_ref.name


def view_inner_cpp_type(field: FieldDef) -> str:
    if field.type_ref.category == "array":
        inner = cpp_type(TypeRef(category="simple", name=field.type_ref.name), False)
        return f"std::vector<{inner}>"
    if field.type_ref.category == "map":
        inner = cpp_type(TypeRef(category="simple", name=field.type_ref.value_name), False)
        return f"std::map<std::string, {inner}>"
    return cpp_type(TypeRef(category="simple", name=field.type_ref.name), False)


def header_text(doc: SchemaDoc, output_path: pathlib.Path) -> str:
    ns = cpp_namespace(doc)
    defs = {item.name: item for item in doc.defs}
    ordered_defs: list[DefBlock] = []
    visiting: set[str] = set()
    visited: set[str] = set()

    def field_dependencies(item: FieldDef) -> list[str]:
        if item.type_ref.category == "simple":
            candidate = item.type_ref.name
        elif item.type_ref.category == "array":
            candidate = item.type_ref.name
        else:
            candidate = item.type_ref.value_name
        return [candidate] if candidate in defs else []

    def visit(def_name: str) -> None:
        if def_name in visited:
            return
        if def_name in visiting:
            raise ValueError(f"cyclic schema dependency: {def_name}")
        visiting.add(def_name)
        for field in defs[def_name].fields:
            for dep in field_dependencies(field):
                visit(dep)
        visiting.remove(def_name)
        visited.add(def_name)
        ordered_defs.append(defs[def_name])

    for def_name in defs:
        visit(def_name)

    lines: list[str] = []
    guard = guard_name(doc.schema_name)
    lines.extend([
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "basement/schema/schema_codec_helpers.h"',
        '#include "basement/schema/schema_registry.h"',
        '#include "basement/transport/common/fixed_layout_codec.h"',
        "",
        "#include <cstring>",
        "#include <optional>",
        "#include <span>",
        "#include <utility>",
        "",
    ])
    lines.append(f"namespace {'::'.join(ns)} {{")
    lines.append("")
    lines.extend([
        "using star::schema::schema_read;",
        "using star::schema::schema_write;",
        "",
    ])
    lines.extend([
        "inline auto _schema_type_id() -> star::schema::SchemaTypeId {",
        "    auto out = star::schema::SchemaTypeId{};",
        f'    out.name = "{doc.schema_name}";',
        f"    out.version = {doc.version};",
        f"    out.hash = static_cast<uint64_t>({doc.hash_value});",
        "    return out;",
        "}",
        "",
    ])

    def emit_field_builder(field: FieldDef, prefix: str) -> list[str]:
        out_lines = [
            f"{prefix}field.id = {field.id};",
            f'{prefix}field.name = "{field.name}";',
            f"{prefix}field.kind = {field_kind_expr(field.type_ref, defs)};",
            f'{prefix}field.type_name = "{field_type_name(field.type_ref)}";',
            f"{prefix}field.optional = {'true' if field.type_ref.optional else 'false'};",
            f"{prefix}field.explicit_required = {'true' if field.type_ref.explicit_required else 'false'};",
            f"{prefix}field.repeated = {'true' if field.type_ref.category == 'array' else 'false'};",
        ]
        if field.type_ref.category == "map":
            out_lines.extend([
                f"{prefix}field.has_value_kind = true;",
                f"{prefix}field.value_kind = {field_kind_expr(TypeRef(category='simple', name=field.type_ref.value_name), defs)};",
                f'{prefix}field.value_type_name = "{field.type_ref.value_name}";',
            ])
            if field.type_ref.value_name in defs:
                out_lines.append(f"{prefix}field.value_children = _build_fields_{field.type_ref.value_name}();")
        elif field.type_ref.name in defs:
            out_lines.append(f"{prefix}field.children = _build_fields_{field.type_ref.name}();")
        if field.default is not None:
            out_lines.append(f"{prefix}field.default_value = {descriptor_default_expr(field)};")
        return out_lines

    for block in ordered_defs:
        lines.append(f"inline auto _build_fields_{block.name}() -> std::vector<star::schema::FieldDescriptor> {{")
        lines.append("    auto out = std::vector<star::schema::FieldDescriptor>{};")
        for field in block.fields:
            lines.append("    {")
            lines.append("        auto field = star::schema::FieldDescriptor{};")
            lines.extend(emit_field_builder(field, "        "))
            lines.append("        out.emplace_back(std::move(field));")
            lines.append("    }")
        lines.append("    return out;")
        lines.append("}")
        lines.append("")
    lines.extend([
        "inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {",
        "    auto out = star::schema::SchemaDescriptor{};",
        "    out.type_id = _schema_type_id();",
        f'    out.namespace_name = "{doc.namespace_name}";',
        f'    out.root_name = "{doc.root_name}";',
        f"    out.allow_unknown_fields = {'true' if doc.allow_unknown else 'false'};",
        f"    out.fields = _build_fields_{doc.root_name}();",
        "    return out;",
        "}",
        "",
    ])
    for block in ordered_defs:
        def_name = block.name
        lines.append(f"struct {def_name}Builder;")
        lines.append("")
        lines.append(f"struct {def_name} {{")
        for field in block.fields:
            if field.id > 0:
                lines.append(f"    static constexpr uint32_t kFieldId_{field.name} = {field.id};")
        if any(field.id > 0 for field in block.fields):
            lines.append("")
        for field in block.fields:
            init = default_expr(field)
            if init:
                lines.append(f"    {cpp_type(field.type_ref, field.default is not None)} {field.name}{{{init}}};")
            else:
                lines.append(f"    {cpp_type(field.type_ref, field.default is not None)} {field.name}{{}};")
        if def_name == doc.root_name:
            lines.append("    star::schema::UnknownFieldSet unknown_fields_{};")
        lines.append("")
        for field in block.fields:
            if field.type_ref.optional and field.default is None:
                lines.append(f"    auto has_{field.name}() const -> bool {{")
                lines.append(f"        return {field.name}.has_value();")
                lines.append("    }")
                lines.append("")
        lines.append(f"    static auto from_value(const star::schema::SchemaValue& value) -> {def_name} {{")
        lines.append(f"        auto out = {def_name}{{}};")
        lines.append("        const auto* obj = value.get_object_ptr();")
        lines.append("        if (obj == nullptr) {")
        lines.append('            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");')
        lines.append("        }")
        for field in block.fields:
            lines.append(f'        if (const auto it = obj->find("{field.name}"); it != obj->end()) {{')
            if field.type_ref.optional and field.default is None:
                lines.append(f"            out.{field.name} = {read_expr(field)};")
            else:
                lines.append(f"            out.{field.name} = {read_expr(field)};")
            lines.append("        } else {")
            if field.type_ref.optional and field.default is None:
                lines.append(f"            out.{field.name}.reset();")
            elif field.default is None:
                lines.append(f'            throw star::excep::RuntimeError("schema generated type decode failed, missing field: {field.name}");')
            lines.append("        }")
        lines.append("        return out;")
        lines.append("    }")
        lines.append("")
        lines.append("    auto to_value() const -> star::schema::SchemaValue {")
        lines.append("        auto obj = star::schema::SchemaObject{};")
        for field in block.fields:
            if field.type_ref.optional and field.default is None:
                lines.append(f"        if ({field.name}.has_value()) {{")
                lines.append(f'            obj.emplace("{field.name}", schema_write(*{field.name}));')
                lines.append("        }")
            else:
                lines.append(f'        obj.emplace("{field.name}", schema_write({field.name}));')
        lines.append("        return star::schema::SchemaValue(std::move(obj));")
        lines.append("    }")
        if def_name == doc.root_name:
            lines.append("")
            lines.append("    static auto schema_type_id() -> star::schema::SchemaTypeId {")
            lines.append("        return _schema_type_id();")
            lines.append("    }")
            lines.append("")
            lines.append("    static auto schema_descriptor() -> star::schema::SchemaDescriptor {")
            lines.append("        return _schema_descriptor();")
            lines.append("    }")
            lines.append("")
            lines.append("    static void register_schema(star::schema::SchemaRegistry& registry) {")
            lines.append("        registry.register_schema(schema_descriptor());")
            lines.append("    }")
            lines.append("")
            lines.append("    auto encode_binary() const -> std::vector<uint8_t> {")
            lines.append("        return star::schema::BinaryCodec::encode(_schema_descriptor(), to_value(), unknown_fields_);")
            lines.append("    }")
            lines.append("")
            lines.append("    static auto from_binary(std::span<const uint8_t> bytes) -> " + def_name + " {")
            lines.append("        auto decoded = star::schema::BinaryCodec::decode_with_unknown(_schema_descriptor(), bytes);")
            lines.append("        auto out = from_value(decoded.value);")
            lines.append("        out.unknown_fields_ = std::move(decoded.unknown_fields);")
            lines.append("        return out;")
            lines.append("    }")
            lines.append("")
            lines.append("    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {")
            lines.append("        return unknown_fields_;")
            lines.append("    }")
            lines.append("")
            lines.append("    void set_unknown_fields(star::schema::UnknownFieldSet unknown_fields) {")
            lines.append("        unknown_fields_ = std::move(unknown_fields);")
            lines.append("    }")
        lines.append("")
        lines.append("    auto encode_fixed_layout() const -> std::vector<uint8_t>;")
        lines.append("")
        lines.append("    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;")
        lines.append("")
        lines.append(f"    static auto from_fixed_layout(std::span<const uint8_t> data) -> {def_name};")
        lines.append("};")
        lines.append("")
        lines.append(f"struct {def_name}View {{")
        lines.append(f"    static constexpr size_t kFieldCount = {len(block.fields)};")
        lines.append("")
        lines.append(f"    {def_name}View() = default;")
        lines.append(f"    explicit {def_name}View(const uint8_t* data, size_t size) : view_(data, size) {{}}")
        lines.append(f"    explicit {def_name}View(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {{}}")
        lines.append("")
        lines.append(f"    auto valid() const -> bool {{ return view_.valid(); }}")
        if def_name == doc.root_name:
            lines.append("")
            lines.append(f"    static auto FromBinary(std::span<const uint8_t> bytes) -> {def_name}View {{")
            lines.append(f"        return {def_name}View(bytes);")
            lines.append("    }")
        lines.append("")
        for i, field in enumerate(block.fields):
            lines.append(f"    static constexpr size_t kSlotIndex_{field.name} = {i};")
        if block.fields:
            lines.append("")
        for i, field in enumerate(block.fields):
            if field.type_ref.optional and field.default is None:
                lines.append(f"    auto Has_{field.name}() const -> bool {{")
                lines.append(f"        return not view_.is_null({i});")
                lines.append("    }")
                lines.append("")
            else:
                lines.append(f"    auto Has_{field.name}() const -> bool {{")
                lines.append(f"        return not view_.is_null({i});")
                lines.append("    }")
                lines.append("")
            if field.type_ref.category == "array":
                if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64"}:
                    lines.append(f"    auto {field.name}() const -> std::vector<int64_t> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::transport::decode_primitive_array<int64_t>(*v) : std::vector<int64_t>{{}};")
                    lines.append("    }")
                elif field.type_ref.name in {"float", "double", "float64"}:
                    lines.append(f"    auto {field.name}() const -> std::vector<double> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::transport::decode_primitive_array<double>(*v) : std::vector<double>{{}};")
                    lines.append("    }")
                elif field.type_ref.name == "bool":
                    lines.append(f"    auto {field.name}() const -> std::vector<bool> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::transport::decode_bool_array(*v) : std::vector<bool>{{}};")
                    lines.append("    }")
                elif field.type_ref.name == "string":
                    lines.append(f"    auto {field.name}() const -> std::vector<std::string> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{{}};")
                    lines.append("    }")
                else:
                    lines.append(f"    auto {field.name}_bytes() const -> std::span<const uint8_t> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? *v : std::span<const uint8_t>{{}};")
                    lines.append("    }")
            elif field.type_ref.category == "map":
                if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64"}:
                    lines.append(f"    auto {field.name}() const -> std::map<std::string, int64_t> {{")
                    lines.append(f"        auto v = view_.get_map_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::transport::decode_primitive_map<int64_t>(*v) : std::map<std::string, int64_t>{{}};")
                    lines.append("    }")
                elif field.type_ref.value_name in {"float", "double", "float64"}:
                    lines.append(f"    auto {field.name}() const -> std::map<std::string, double> {{")
                    lines.append(f"        auto v = view_.get_map_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::transport::decode_primitive_map<double>(*v) : std::map<std::string, double>{{}};")
                    lines.append("    }")
                elif field.type_ref.value_name == "string":
                    lines.append(f"    auto {field.name}() const -> std::map<std::string, std::string> {{")
                    lines.append(f"        auto v = view_.get_map_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::transport::decode_string_map(*v) : std::map<std::string, std::string>{{}};")
                    lines.append("    }")
                else:
                    lines.append(f"    auto {field.name}_bytes() const -> std::span<const uint8_t> {{")
                    lines.append(f"        auto v = view_.get_map_bytes({i});")
                    lines.append(f"        return v.has_value() ? *v : std::span<const uint8_t>{{}};")
                    lines.append("    }")
            elif field.type_ref.name == "bool":
                lines.append(f"    auto {field.name}() const -> bool {{")
                lines.append(f"        auto v = view_.get_bool({i});")
                lines.append(f"        return v.has_value() ? *v : false;")
                lines.append("    }")
            elif field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64"}:
                lines.append(f"    auto {field.name}() const -> int64_t {{")
                lines.append(f"        auto v = view_.get_int64({i});")
                lines.append(f"        return v.has_value() ? *v : int64_t{{0}};")
                lines.append("    }")
            elif field.type_ref.name in {"float", "double", "float64"}:
                lines.append(f"    auto {field.name}() const -> double {{")
                lines.append(f"        auto v = view_.get_float64({i});")
                lines.append(f"        return v.has_value() ? *v : 0.0;")
                lines.append("    }")
            elif field.type_ref.name == "string":
                lines.append(f"    auto {field.name}() const -> std::string_view {{")
                lines.append(f"        auto v = view_.get_string({i});")
                lines.append(f"        return v.has_value() ? *v : std::string_view{{}};")
                lines.append("    }")
            elif _is_schema_text_type(field.type_ref.name):
                lines.append(f"    auto {field.name}() const -> std::string_view {{")
                lines.append(f"        auto v = view_.get_string({i});")
                lines.append(f"        return v.has_value() ? *v : std::string_view{{}};")
                lines.append("    }")
            else:
                lines.append(f"    auto {field.name}_bytes() const -> std::span<const uint8_t> {{")
                lines.append(f"        auto v = view_.get_object_bytes({i});")
                lines.append(f"        return v.has_value() ? *v : std::span<const uint8_t>{{}};")
                lines.append("    }")
            lines.append("")
        lines.append("    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }")
        lines.append("")
        lines.append("  private:")
        lines.append("    star::transport::FixedLayoutView view_;")
        lines.append("};")
        lines.append("")
        lines.append(f"struct {def_name}Builder {{")
        lines.append(f"    static constexpr size_t kFieldCount = {len(block.fields)};")
        lines.append("")
        lines.append(f"    {def_name}Builder() : writer_({len(block.fields)}) {{}}")
        lines.append("")
        for i, field in enumerate(block.fields):
            if field.type_ref.category == "array":
                if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64"}:
                    lines.append(f"    void set_{field.name}(const std::vector<int64_t>& value) {{ auto bytes = star::transport::encode_primitive_array<int64_t>(value); writer_.set_array({i}, bytes); }}")
                elif field.type_ref.name in {"float", "double", "float64"}:
                    lines.append(f"    void set_{field.name}(const std::vector<double>& value) {{ auto bytes = star::transport::encode_primitive_array<double>(value); writer_.set_array({i}, bytes); }}")
                elif field.type_ref.name == "bool":
                    lines.append(f"    void set_{field.name}(const std::vector<bool>& value) {{ auto bytes = star::transport::encode_bool_array(value); writer_.set_array({i}, bytes); }}")
                elif field.type_ref.name == "string":
                    lines.append(f"    void set_{field.name}(const std::vector<std::string>& value) {{ auto bytes = star::transport::encode_string_array(value); writer_.set_array({i}, bytes); }}")
                else:
                    lines.append(f"    void set_{field.name}_bytes(std::span<const uint8_t> value) {{ writer_.set_array({i}, value); }}")
            elif field.type_ref.category == "map":
                if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64"}:
                    lines.append(f"    void set_{field.name}(const std::map<std::string, int64_t>& value) {{ auto bytes = star::transport::encode_primitive_map<int64_t>(value); writer_.set_map({i}, bytes); }}")
                elif field.type_ref.value_name in {"float", "double", "float64"}:
                    lines.append(f"    void set_{field.name}(const std::map<std::string, double>& value) {{ auto bytes = star::transport::encode_primitive_map<double>(value); writer_.set_map({i}, bytes); }}")
                elif field.type_ref.value_name == "string":
                    lines.append(f"    void set_{field.name}(const std::map<std::string, std::string>& value) {{ auto bytes = star::transport::encode_string_map(value); writer_.set_map({i}, bytes); }}")
                else:
                    lines.append(f"    void set_{field.name}_bytes(std::span<const uint8_t> value) {{ writer_.set_map({i}, value); }}")
            elif field.type_ref.name == "bool":
                lines.append(f"    void set_{field.name}(bool value) {{ writer_.set_bool({i}, value); }}")
            elif field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64"}:
                lines.append(f"    void set_{field.name}(int64_t value) {{ writer_.set_int64({i}, value); }}")
            elif field.type_ref.name in {"float", "double", "float64"}:
                lines.append(f"    void set_{field.name}(double value) {{ writer_.set_float64({i}, value); }}")
            elif field.type_ref.name == "string":
                lines.append(f"    void set_{field.name}(std::string_view value) {{ writer_.set_string({i}, value); }}")
            elif _is_schema_text_type(field.type_ref.name):
                lines.append(f"    void set_{field.name}(std::string_view value) {{ writer_.set_string({i}, value); }}")
            else:
                lines.append(f"    void set_{field.name}_bytes(std::span<const uint8_t> value) {{ writer_.set_object({i}, value); }}")
        if block.fields:
            lines.append("")
        for i, field in enumerate(block.fields):
            if field.type_ref.optional and field.default is None:
                lines.append(f"    void clear_{field.name}() {{ writer_.set_null({i}); }}")
        if any(f.type_ref.optional and f.default is None for f in block.fields):
            lines.append("")
        lines.append("    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {")
        lines.append("        return writer_.serialize_into(buf, cap);")
        lines.append("    }")
        lines.append("")
        lines.append("    auto EncodeTo(std::span<uint8_t> out) const -> size_t {")
        lines.append("        return EncodeTo(out.data(), out.size());")
        lines.append("    }")
        lines.append("")
        lines.append("    auto Serialize() const -> std::vector<uint8_t> {")
        lines.append("        return writer_.serialize();")
        lines.append("    }")
        lines.append("")
        lines.append("  private:")
        lines.append("    star::transport::FixedLayoutWriter writer_;")
        lines.append("};")
        lines.append("")
        def _gen_field_set(field, is_optional, indent):
            if field.type_ref.category == "array":
                if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "bool", "string"}:
                    lines.append(f"{indent}builder.set_{field.name}({'*' + field.name if is_optional else field.name});")
                elif _is_binary_only_type(field.type_ref.name):
                    lines.append(f"{indent}{{ auto bytes = star::transport::encode_binary_value_array<{_cpp_type_for_binary_only(field.type_ref.name)}>({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
                else:
                    lines.append(f"{indent}{{ auto bytes = star::transport::encode_object_array({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
            elif field.type_ref.category == "map":
                if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                    lines.append(f"{indent}builder.set_{field.name}({'*' + field.name if is_optional else field.name});")
                elif _is_binary_only_type(field.type_ref.value_name):
                    lines.append(f"{indent}{{ auto bytes = star::transport::encode_binary_value_map<{_cpp_type_for_binary_only(field.type_ref.value_name)}>({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
                else:
                    lines.append(f"{indent}{{ auto bytes = star::transport::encode_object_map({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
            elif field.type_ref.name in {"bool", "int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                lines.append(f"{indent}builder.set_{field.name}({'*' + field.name if is_optional else field.name});")
            elif _is_binary_only_type(field.type_ref.name):
                lines.append(f"{indent}{{ auto bytes = {field.name + '->' if is_optional else field.name + '.'}encode_binary(); builder.set_{field.name}_bytes(bytes); }}")
            elif _is_schema_text_type(field.type_ref.name):
                lines.append(f"{indent}{{ auto text = star::schema::dump_schema_text(star::schema::SchemaValue({'*' + field.name if is_optional else field.name})); builder.set_{field.name}(text); }}")
            else:
                lines.append(f"{indent}{{ auto bytes = {field.name + '->' if is_optional else field.name + '.'}encode_fixed_layout(); builder.set_{field.name}_bytes(bytes); }}")
        lines.append(f"inline auto {def_name}::encode_fixed_layout() const -> std::vector<uint8_t> {{")
        lines.append(f"    auto builder = {def_name}Builder();")
        for i, field in enumerate(block.fields):
            if field.type_ref.optional and field.default is None:
                lines.append(f"    if ({field.name}.has_value()) {{")
                _gen_field_set(field, True, "        ")
                lines.append("    } else {")
                lines.append(f"        builder.clear_{field.name}();")
                lines.append("    }")
            else:
                _gen_field_set(field, False, "    ")
        lines.append("    return builder.Serialize();")
        lines.append("}")
        lines.append("")
        lines.append(f"inline auto {def_name}::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {{")
        lines.append(f"    auto builder = {def_name}Builder();")
        for i, field in enumerate(block.fields):
            if field.type_ref.optional and field.default is None:
                lines.append(f"    if ({field.name}.has_value()) {{")
                _gen_field_set(field, True, "        ")
                lines.append("    } else {")
                lines.append(f"        builder.clear_{field.name}();")
                lines.append("    }")
            else:
                _gen_field_set(field, False, "    ")
        lines.append("    return builder.EncodeTo(buf, cap);")
        lines.append("}")
        lines.append("")
        lines.append(f"inline auto {def_name}::from_fixed_layout(std::span<const uint8_t> data) -> {def_name} {{")
        lines.append(f"    auto view = {def_name}View(data.data(), data.size());")
        lines.append(f"    auto out = {def_name}();")
        for i, field in enumerate(block.fields):
            if field.type_ref.optional and field.default is None:
                if field.type_ref.category == "array":
                    if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "bool", "string"}:
                        lines.append(f"    if (view.Has_{field.name}()) {{ out.{field.name} = view.{field.name}(); }}")
                    elif _is_binary_only_type(field.type_ref.name):
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_binary_value_array<{_cpp_type_for_binary_only(field.type_ref.name)}>(bytes); }}")
                    else:
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_object_array<{field.type_ref.name}>(bytes); }}")
                elif field.type_ref.category == "map":
                    if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                        lines.append(f"    if (view.Has_{field.name}()) {{ out.{field.name} = view.{field.name}(); }}")
                    elif _is_binary_only_type(field.type_ref.value_name):
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_binary_value_map<{_cpp_type_for_binary_only(field.type_ref.value_name)}>(bytes); }}")
                    else:
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_object_map<{field.type_ref.value_name}>(bytes); }}")
                elif field.type_ref.name in {"bool", "int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                    lines.append(f"    if (view.Has_{field.name}()) {{ out.{field.name} = view.{field.name}(); }}")
                elif _is_binary_only_type(field.type_ref.name):
                    lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = {_cpp_type_for_binary_only(field.type_ref.name)}::from_binary(bytes); }}")
                elif _is_schema_text_type(field.type_ref.name):
                    lines.append(f"    if (view.Has_{field.name}()) {{ auto text = view.{field.name}(); auto val = star::schema::parse_schema_text(text); out.{field.name} = val.as_object(); }}")
                else:
                    lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = {field.type_ref.name}::from_fixed_layout(bytes); }}")
            else:
                if field.type_ref.category == "array":
                    if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "bool", "string"}:
                        lines.append(f"    out.{field.name} = view.{field.name}();")
                    elif _is_binary_only_type(field.type_ref.name):
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_binary_value_array<{_cpp_type_for_binary_only(field.type_ref.name)}>(bytes); }}")
                    else:
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_object_array<{field.type_ref.name}>(bytes); }}")
                elif field.type_ref.category == "map":
                    if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                        lines.append(f"    out.{field.name} = view.{field.name}();")
                    elif _is_binary_only_type(field.type_ref.value_name):
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_binary_value_map<{_cpp_type_for_binary_only(field.type_ref.value_name)}>(bytes); }}")
                    else:
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::transport::decode_object_map<{field.type_ref.value_name}>(bytes); }}")
                elif field.type_ref.name in {"bool", "int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                    lines.append(f"    out.{field.name} = view.{field.name}();")
                elif _is_binary_only_type(field.type_ref.name):
                    lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = {_cpp_type_for_binary_only(field.type_ref.name)}::from_binary(bytes); }}")
                elif _is_schema_text_type(field.type_ref.name):
                    lines.append(f"    {{ auto text = view.{field.name}(); auto val = star::schema::parse_schema_text(text); out.{field.name} = val.as_object(); }}")
                else:
                    lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = {field.type_ref.name}::from_fixed_layout(bytes); }}")
        lines.append("    return out;")
        lines.append("}")
        lines.append("")
    lines.append(f"using {doc.root_name}Schema = {doc.root_name};")
    lines.append("")
    lines.append(f"}}  // namespace {'::'.join(ns)}")
    lines.append("")
    lines.append(f"#endif // {guard}")
    lines.append("")
    return "\n".join(lines)


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
    lines.append("}")
    return lines


def format_schema(doc: SchemaDoc) -> str:
    lines: list[str] = [f"schema {doc.schema_name}"]
    root_block = next((block for block in doc.defs if block.name == doc.root_name), None)
    if root_block is None:
        lines.append(f"root {doc.root_name}")
    else:
        lines.append("")
        lines.extend(format_block("root", root_block))
    if doc.namespace_name:
        lines.append("")
        lines.append(f"namespace {doc.namespace_name}")
    if doc.version:
        lines.append(f"version {doc.version}")
    if doc.hash_value:
        lines.append(f"hash {doc.hash_value}")
    if doc.allow_unknown:
        lines.append("allow_unknown true")

    other_defs = [block for block in doc.defs if block.name != doc.root_name]
    if other_defs:
        lines.append("")
        for index, block in enumerate(other_defs):
            if index > 0:
                lines.append("")
            lines.extend(format_block("def", block))
    lines.append("")
    return "\n".join(lines)


def run_gen(schema: pathlib.Path, output: pathlib.Path) -> int:
    doc = Parser(schema.read_text(encoding="utf-8")).parse()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(header_text(doc, output), encoding="utf-8")
    return 0


def run_gen_dir(schema_dir: pathlib.Path, output_dir: pathlib.Path) -> int:
    for schema_path in sorted(schema_dir.rglob("*.schema")):
        rel = schema_path.relative_to(schema_dir)
        output_path = output_dir / rel.with_suffix(".s.h")
        ret = run_gen(schema_path, output_path)
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


def run_lint(schema: pathlib.Path) -> int:
    try:
        doc = Parser(schema.read_text(encoding="utf-8")).parse()
    except Exception as exc:
        print(f"lint parse error: {exc}", file=sys.stderr)
        return 2

    issues: list[str] = []
    defs = {block.name: block for block in doc.defs}
    if doc.root_name not in defs:
        issues.append(f"root '{doc.root_name}' not found in root/def blocks")

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

    if issues:
        for issue in issues:
            print(f"LINT: {issue}")
        return 1
    print("LINT_OK")
    return 0


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] not in {"gen", "fmt", "compat", "lint"}:
        parser = argparse.ArgumentParser()
        parser.add_argument("--schema", required=True)
        parser.add_argument("--output", required=True)
        args = parser.parse_args()
        return run_gen(pathlib.Path(args.schema), pathlib.Path(args.output))

    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    gen_parser = subparsers.add_parser("gen")
    gen_parser.add_argument("--schema", default=None)
    gen_parser.add_argument("--output", default=None)
    gen_parser.add_argument("--schema-dir", default=None)
    gen_parser.add_argument("--output-dir", default=None)

    fmt_parser = subparsers.add_parser("fmt")
    fmt_parser.add_argument("--schema", required=True)
    fmt_parser.add_argument("--output")
    fmt_parser.add_argument("--in-place", action="store_true")

    compat_parser = subparsers.add_parser("compat")
    compat_parser.add_argument("old_schema")
    compat_parser.add_argument("new_schema")

    lint_parser = subparsers.add_parser("lint")
    lint_parser.add_argument("--schema", required=True)

    args = parser.parse_args()
    if args.command == "gen":
        if args.schema_dir:
            return run_gen_dir(pathlib.Path(args.schema_dir), pathlib.Path(args.output_dir))
        return run_gen(pathlib.Path(args.schema), pathlib.Path(args.output))
    if args.command == "fmt":
        return run_fmt(pathlib.Path(args.schema), pathlib.Path(args.output) if args.output else None, args.in_place)
    if args.command == "compat":
        return run_compat(pathlib.Path(args.old_schema), pathlib.Path(args.new_schema))
    if args.command == "lint":
        return run_lint(pathlib.Path(args.schema))
    raise ValueError(f"unknown command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
