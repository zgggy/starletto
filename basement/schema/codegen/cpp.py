from __future__ import annotations

import pathlib
import re

from .ir import DefBlock, FieldDef, SchemaDoc, TypeRef, schema_content_hash


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
        '#include "basement/schema/fixed_layout_codec.h"',
        '#include "basement/schema/decode_policy.h"',
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
    # helper 名字带 schema 前缀：多个生成头若共享 C++ 命名空间（如 setting.planning
    # 下的 trajectory/motion），同名 def 不会再撞车（ODR）
    helper = "star_" + re.sub(r"[^0-9a-zA-Z_]", "_", doc.schema_name.lower())
    root_set = set(doc.roots)
    single_root = len(doc.roots) == 1

    # 哈希由内容算出，不从文件里抄：声明 `hash` 只是「内容指纹钉子」，
    # gen/lint 会校验它和内容一致；真正写进类型身份的是算出来的值。
    # 多 root：每个 root 单独占一个类型身份，哈希只覆盖该 root 可达的定义。
    for root_name in doc.roots:
        identity = doc.schema_name if single_root else f"{doc.schema_name}.{root_name}"
        content_hash = schema_content_hash(doc, root_name)
        lines.extend([
            f"inline auto {helper}_type_id_{root_name}() -> star::schema::SchemaTypeId {{",
            "    auto out = star::schema::SchemaTypeId{};",
            f'    out.name = "{identity}";',
            f"    out.version = {doc.version};",
            f"    out.hash = 0x{content_hash:016x}ULL;",
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
                out_lines.append(f"{prefix}field.value_children = {helper}_build_fields_{field.type_ref.value_name}();")
        elif field.type_ref.name in defs:
            out_lines.append(f"{prefix}field.children = {helper}_build_fields_{field.type_ref.name}();")
        if field.default is not None:
            out_lines.append(f"{prefix}field.default_value = {descriptor_default_expr(field)};")
        return out_lines

    for block in ordered_defs:
        lines.append(f"inline auto {helper}_build_fields_{block.name}() -> std::vector<star::schema::FieldDescriptor> {{")
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
    for root_name in doc.roots:
        lines.extend([
            f"inline auto {helper}_descriptor_{root_name}() -> star::schema::SchemaDescriptor {{",
            "    auto out = star::schema::SchemaDescriptor{};",
            f"    out.type_id = {helper}_type_id_{root_name}();",
            f'    out.namespace_name = "{doc.namespace_name}";',
            f'    out.root_name = "{root_name}";',
            f"    out.allow_unknown_fields = {'true' if doc.allow_unknown else 'false'};",
            f"    out.fields = {helper}_build_fields_{root_name}();",
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
        if def_name in root_set:
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
        if def_name in root_set:
            lines.append("")
            lines.append("    static auto schema_type_id() -> star::schema::SchemaTypeId {")
            lines.append(f"        return {helper}_type_id_{def_name}();")
            lines.append("    }")
            lines.append("")
            lines.append("    static auto schema_descriptor() -> star::schema::SchemaDescriptor {")
            lines.append(f"        return {helper}_descriptor_{def_name}();")
            lines.append("    }")
            lines.append("")
            lines.append("    static void register_schema(star::schema::SchemaRegistry& registry) {")
            lines.append("        registry.register_schema(schema_descriptor());")
            lines.append("    }")
            lines.append("")
            lines.append("    auto encode_binary() const -> std::vector<uint8_t> {")
            lines.append(f"        return star::schema::BinaryCodec::encode({helper}_descriptor_{def_name}(), to_value(), unknown_fields_);")
            lines.append("    }")
            lines.append("")
            lines.append("    static auto from_binary_with_policy(std::span<const uint8_t> bytes, const star::schema::DecodePolicy& policy) -> " + def_name + " {")
            lines.append(f"        auto decoded = star::schema::BinaryCodec::decode_with_unknown({helper}_descriptor_{def_name}(), bytes, policy);")
            lines.append("        auto out = from_value(decoded.value);")
            lines.append("        out.unknown_fields_ = std::move(decoded.unknown_fields);")
            lines.append("        return out;")
            lines.append("    }")
            lines.append("")
            lines.append("    static auto from_binary(std::span<const uint8_t> bytes) -> " + def_name + " {")
            lines.append("        return from_binary_with_policy(bytes, star::schema::DecodePolicy{});")
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
        lines.append("")
        lines.append("    auto schema_version() const -> int64_t { return view_.schema_version(); }")
        lines.append("")
        lines.append("    auto schema_hash() const -> uint64_t { return view_.schema_hash(); }")
        lines.append("")
        lines.append("    auto matches(const star::schema::SchemaTypeId& type_id) const -> bool { return view_.matches(type_id); }")
        if def_name in root_set:
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
                    lines.append(f"        return v.has_value() ? star::schema::decode_primitive_array<int64_t>(*v) : std::vector<int64_t>{{}};")
                    lines.append("    }")
                elif field.type_ref.name in {"float", "double", "float64"}:
                    lines.append(f"    auto {field.name}() const -> std::vector<double> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::schema::decode_primitive_array<double>(*v) : std::vector<double>{{}};")
                    lines.append("    }")
                elif field.type_ref.name == "bool":
                    lines.append(f"    auto {field.name}() const -> std::vector<bool> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::schema::decode_bool_array(*v) : std::vector<bool>{{}};")
                    lines.append("    }")
                elif field.type_ref.name == "string":
                    lines.append(f"    auto {field.name}() const -> std::vector<std::string> {{")
                    lines.append(f"        auto v = view_.get_array_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::schema::decode_string_array(*v) : std::vector<std::string>{{}};")
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
                    lines.append(f"        return v.has_value() ? star::schema::decode_primitive_map<int64_t>(*v) : std::map<std::string, int64_t>{{}};")
                    lines.append("    }")
                elif field.type_ref.value_name in {"float", "double", "float64"}:
                    lines.append(f"    auto {field.name}() const -> std::map<std::string, double> {{")
                    lines.append(f"        auto v = view_.get_map_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::schema::decode_primitive_map<double>(*v) : std::map<std::string, double>{{}};")
                    lines.append("    }")
                elif field.type_ref.value_name == "string":
                    lines.append(f"    auto {field.name}() const -> std::map<std::string, std::string> {{")
                    lines.append(f"        auto v = view_.get_map_bytes({i});")
                    lines.append(f"        return v.has_value() ? star::schema::decode_string_map(*v) : std::map<std::string, std::string>{{}};")
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
        lines.append("    auto raw_view() const -> const star::schema::FixedLayoutView& { return view_; }")
        lines.append("")
        lines.append("  private:")
        lines.append("    star::schema::FixedLayoutView view_;")
        lines.append("};")
        lines.append("")
        lines.append(f"struct {def_name}Builder {{")
        lines.append(f"    static constexpr size_t kFieldCount = {len(block.fields)};")
        lines.append("")
        fixed_layout_root = def_name if def_name in root_set else doc.roots[0]
        lines.append(f"    {def_name}Builder() : writer_({len(block.fields)}, {helper}_type_id_{fixed_layout_root}()) {{}}")
        lines.append("")
        for i, field in enumerate(block.fields):
            if field.type_ref.category == "array":
                if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64"}:
                    lines.append(f"    void set_{field.name}(const std::vector<int64_t>& value) {{ auto bytes = star::schema::encode_primitive_array<int64_t>(value); writer_.set_array({i}, bytes); }}")
                elif field.type_ref.name in {"float", "double", "float64"}:
                    lines.append(f"    void set_{field.name}(const std::vector<double>& value) {{ auto bytes = star::schema::encode_primitive_array<double>(value); writer_.set_array({i}, bytes); }}")
                elif field.type_ref.name == "bool":
                    lines.append(f"    void set_{field.name}(const std::vector<bool>& value) {{ auto bytes = star::schema::encode_bool_array(value); writer_.set_array({i}, bytes); }}")
                elif field.type_ref.name == "string":
                    lines.append(f"    void set_{field.name}(const std::vector<std::string>& value) {{ auto bytes = star::schema::encode_string_array(value); writer_.set_array({i}, bytes); }}")
                else:
                    lines.append(f"    void set_{field.name}_bytes(std::span<const uint8_t> value) {{ writer_.set_array({i}, value); }}")
            elif field.type_ref.category == "map":
                if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64"}:
                    lines.append(f"    void set_{field.name}(const std::map<std::string, int64_t>& value) {{ auto bytes = star::schema::encode_primitive_map<int64_t>(value); writer_.set_map({i}, bytes); }}")
                elif field.type_ref.value_name in {"float", "double", "float64"}:
                    lines.append(f"    void set_{field.name}(const std::map<std::string, double>& value) {{ auto bytes = star::schema::encode_primitive_map<double>(value); writer_.set_map({i}, bytes); }}")
                elif field.type_ref.value_name == "string":
                    lines.append(f"    void set_{field.name}(const std::map<std::string, std::string>& value) {{ auto bytes = star::schema::encode_string_map(value); writer_.set_map({i}, bytes); }}")
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
        lines.append("    star::schema::FixedLayoutWriter writer_;")
        lines.append("};")
        lines.append("")
        def _gen_field_set(field, is_optional, indent):
            if field.type_ref.category == "array":
                if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "bool", "string"}:
                    lines.append(f"{indent}builder.set_{field.name}({'*' + field.name if is_optional else field.name});")
                elif _is_binary_only_type(field.type_ref.name):
                    lines.append(f"{indent}{{ auto bytes = star::schema::encode_binary_value_array<{_cpp_type_for_binary_only(field.type_ref.name)}>({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
                else:
                    lines.append(f"{indent}{{ auto bytes = star::schema::encode_object_array({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
            elif field.type_ref.category == "map":
                if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                    lines.append(f"{indent}builder.set_{field.name}({'*' + field.name if is_optional else field.name});")
                elif _is_binary_only_type(field.type_ref.value_name):
                    lines.append(f"{indent}{{ auto bytes = star::schema::encode_binary_value_map<{_cpp_type_for_binary_only(field.type_ref.value_name)}>({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
                else:
                    lines.append(f"{indent}{{ auto bytes = star::schema::encode_object_map({'*' + field.name if is_optional else field.name}); builder.set_{field.name}_bytes(bytes); }}")
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
        fixed_layout_root = def_name if def_name in root_set else doc.roots[0]
        lines.append(f"    const auto expected = {helper}_type_id_{fixed_layout_root}();")
        lines.append("    if (not view.matches(expected)) {")
        lines.append('        throw star::excep::RuntimeError("fixed layout schema mismatch: expected=", expected.name, " v", expected.version, " h", expected.hash, ", frame v", view.schema_version(), " h", view.schema_hash());')
        lines.append("    }")
        lines.append(f"    auto out = {def_name}();")
        for i, field in enumerate(block.fields):
            if field.type_ref.optional and field.default is None:
                if field.type_ref.category == "array":
                    if field.type_ref.name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "bool", "string"}:
                        lines.append(f"    if (view.Has_{field.name}()) {{ out.{field.name} = view.{field.name}(); }}")
                    elif _is_binary_only_type(field.type_ref.name):
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_binary_value_array<{_cpp_type_for_binary_only(field.type_ref.name)}>(bytes); }}")
                    else:
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_object_array<{field.type_ref.name}>(bytes); }}")
                elif field.type_ref.category == "map":
                    if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                        lines.append(f"    if (view.Has_{field.name}()) {{ out.{field.name} = view.{field.name}(); }}")
                    elif _is_binary_only_type(field.type_ref.value_name):
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_binary_value_map<{_cpp_type_for_binary_only(field.type_ref.value_name)}>(bytes); }}")
                    else:
                        lines.append(f"    if (view.Has_{field.name}()) {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_object_map<{field.type_ref.value_name}>(bytes); }}")
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
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_binary_value_array<{_cpp_type_for_binary_only(field.type_ref.name)}>(bytes); }}")
                    else:
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_object_array<{field.type_ref.name}>(bytes); }}")
                elif field.type_ref.category == "map":
                    if field.type_ref.value_name in {"int", "int32", "int64", "uint32", "uint64", "float", "double", "float64", "string"}:
                        lines.append(f"    out.{field.name} = view.{field.name}();")
                    elif _is_binary_only_type(field.type_ref.value_name):
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_binary_value_map<{_cpp_type_for_binary_only(field.type_ref.value_name)}>(bytes); }}")
                    else:
                        lines.append(f"    {{ auto bytes = view.{field.name}_bytes(); out.{field.name} = star::schema::decode_object_map<{field.type_ref.value_name}>(bytes); }}")
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
    for root_name in doc.roots:
        lines.append(f"using {root_name}Schema = {root_name};")
    lines.append("")
    lines.append(f"}}  // namespace {'::'.join(ns)}")
    lines.append("")
    lines.append(f"#endif // {guard}")
    lines.append("")
    return "\n".join(lines)


