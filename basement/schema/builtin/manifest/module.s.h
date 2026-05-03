#ifndef __MANIFEST_MODULE__
#define __MANIFEST_MODULE__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::generated::manifest::module {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "manifest.module";
    out.version = 0;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ModuleInfo() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "version";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "default_backend";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "install_group";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_LayoutInfo() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "config_dir";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "component_dir";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_DependencyInfo() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "modules";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "libraries";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ModuleConf() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "module";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "ModuleInfo";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_ModuleInfo();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "layout";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "LayoutInfo";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_LayoutInfo();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "components";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "dependency";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "DependencyInfo";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_DependencyInfo();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "";
    out.root_name = "ModuleConf";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ModuleConf();
    return out;
}

struct ModuleInfoBuilder;

struct ModuleInfo {
    static constexpr uint32_t kFieldId_name = 1;
    static constexpr uint32_t kFieldId_version = 2;
    static constexpr uint32_t kFieldId_default_backend = 3;
    static constexpr uint32_t kFieldId_install_group = 4;

    std::string name{};
    std::optional<std::string> version{std::nullopt};
    std::optional<std::string> default_backend{std::nullopt};
    std::optional<std::string> install_group{std::nullopt};

    auto has_version() const -> bool {
        return version.has_value();
    }

    auto has_default_backend() const -> bool {
        return default_backend.has_value();
    }

    auto has_install_group() const -> bool {
        return install_group.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ModuleInfo {
        auto out = ModuleInfo{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("name"); it != obj->end()) {
            out.name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: name");
        }
        if (const auto it = obj->find("version"); it != obj->end()) {
            out.version = schema_read<std::string>(it->second);
        } else {
            out.version.reset();
        }
        if (const auto it = obj->find("default_backend"); it != obj->end()) {
            out.default_backend = schema_read<std::string>(it->second);
        } else {
            out.default_backend.reset();
        }
        if (const auto it = obj->find("install_group"); it != obj->end()) {
            out.install_group = schema_read<std::string>(it->second);
        } else {
            out.install_group.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("name", schema_write(name));
        if (version.has_value()) {
            obj.emplace("version", schema_write(*version));
        }
        if (default_backend.has_value()) {
            obj.emplace("default_backend", schema_write(*default_backend));
        }
        if (install_group.has_value()) {
            obj.emplace("install_group", schema_write(*install_group));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ModuleInfo;
};

struct ModuleInfoView {
    static constexpr size_t kFieldCount = 4;

    ModuleInfoView() = default;
    explicit ModuleInfoView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ModuleInfoView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_name = 0;
    static constexpr size_t kSlotIndex_version = 1;
    static constexpr size_t kSlotIndex_default_backend = 2;
    static constexpr size_t kSlotIndex_install_group = 3;

    auto Has_name() const -> bool {
        return not view_.is_null(0);
    }

    auto name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_version() const -> bool {
        return not view_.is_null(1);
    }

    auto version() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_default_backend() const -> bool {
        return not view_.is_null(2);
    }

    auto default_backend() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_install_group() const -> bool {
        return not view_.is_null(3);
    }

    auto install_group() const -> std::string_view {
        auto v = view_.get_string(3);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ModuleInfoBuilder {
    static constexpr size_t kFieldCount = 4;

    ModuleInfoBuilder() : writer_(4) {}

    void set_name(std::string_view value) { writer_.set_string(0, value); }
    void set_version(std::string_view value) { writer_.set_string(1, value); }
    void set_default_backend(std::string_view value) { writer_.set_string(2, value); }
    void set_install_group(std::string_view value) { writer_.set_string(3, value); }

    void clear_version() { writer_.set_null(1); }
    void clear_default_backend() { writer_.set_null(2); }
    void clear_install_group() { writer_.set_null(3); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto ModuleInfo::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ModuleInfoBuilder();
    builder.set_name(name);
    if (version.has_value()) {
        builder.set_version(*version);
    } else {
        builder.clear_version();
    }
    if (default_backend.has_value()) {
        builder.set_default_backend(*default_backend);
    } else {
        builder.clear_default_backend();
    }
    if (install_group.has_value()) {
        builder.set_install_group(*install_group);
    } else {
        builder.clear_install_group();
    }
    return builder.Serialize();
}

inline auto ModuleInfo::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ModuleInfoBuilder();
    builder.set_name(name);
    if (version.has_value()) {
        builder.set_version(*version);
    } else {
        builder.clear_version();
    }
    if (default_backend.has_value()) {
        builder.set_default_backend(*default_backend);
    } else {
        builder.clear_default_backend();
    }
    if (install_group.has_value()) {
        builder.set_install_group(*install_group);
    } else {
        builder.clear_install_group();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ModuleInfo::from_fixed_layout(std::span<const uint8_t> data) -> ModuleInfo {
    auto view = ModuleInfoView(data.data(), data.size());
    auto out = ModuleInfo();
    out.name = view.name();
    if (view.Has_version()) { out.version = view.version(); }
    if (view.Has_default_backend()) { out.default_backend = view.default_backend(); }
    if (view.Has_install_group()) { out.install_group = view.install_group(); }
    return out;
}

struct LayoutInfoBuilder;

struct LayoutInfo {
    static constexpr uint32_t kFieldId_config_dir = 1;
    static constexpr uint32_t kFieldId_component_dir = 2;

    std::optional<std::string> config_dir{std::nullopt};
    std::optional<std::string> component_dir{std::nullopt};

    auto has_config_dir() const -> bool {
        return config_dir.has_value();
    }

    auto has_component_dir() const -> bool {
        return component_dir.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> LayoutInfo {
        auto out = LayoutInfo{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("config_dir"); it != obj->end()) {
            out.config_dir = schema_read<std::string>(it->second);
        } else {
            out.config_dir.reset();
        }
        if (const auto it = obj->find("component_dir"); it != obj->end()) {
            out.component_dir = schema_read<std::string>(it->second);
        } else {
            out.component_dir.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (config_dir.has_value()) {
            obj.emplace("config_dir", schema_write(*config_dir));
        }
        if (component_dir.has_value()) {
            obj.emplace("component_dir", schema_write(*component_dir));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> LayoutInfo;
};

struct LayoutInfoView {
    static constexpr size_t kFieldCount = 2;

    LayoutInfoView() = default;
    explicit LayoutInfoView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit LayoutInfoView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_config_dir = 0;
    static constexpr size_t kSlotIndex_component_dir = 1;

    auto Has_config_dir() const -> bool {
        return not view_.is_null(0);
    }

    auto config_dir() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_component_dir() const -> bool {
        return not view_.is_null(1);
    }

    auto component_dir() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct LayoutInfoBuilder {
    static constexpr size_t kFieldCount = 2;

    LayoutInfoBuilder() : writer_(2) {}

    void set_config_dir(std::string_view value) { writer_.set_string(0, value); }
    void set_component_dir(std::string_view value) { writer_.set_string(1, value); }

    void clear_config_dir() { writer_.set_null(0); }
    void clear_component_dir() { writer_.set_null(1); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto LayoutInfo::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = LayoutInfoBuilder();
    if (config_dir.has_value()) {
        builder.set_config_dir(*config_dir);
    } else {
        builder.clear_config_dir();
    }
    if (component_dir.has_value()) {
        builder.set_component_dir(*component_dir);
    } else {
        builder.clear_component_dir();
    }
    return builder.Serialize();
}

inline auto LayoutInfo::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = LayoutInfoBuilder();
    if (config_dir.has_value()) {
        builder.set_config_dir(*config_dir);
    } else {
        builder.clear_config_dir();
    }
    if (component_dir.has_value()) {
        builder.set_component_dir(*component_dir);
    } else {
        builder.clear_component_dir();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto LayoutInfo::from_fixed_layout(std::span<const uint8_t> data) -> LayoutInfo {
    auto view = LayoutInfoView(data.data(), data.size());
    auto out = LayoutInfo();
    if (view.Has_config_dir()) { out.config_dir = view.config_dir(); }
    if (view.Has_component_dir()) { out.component_dir = view.component_dir(); }
    return out;
}

struct DependencyInfoBuilder;

struct DependencyInfo {
    static constexpr uint32_t kFieldId_modules = 1;
    static constexpr uint32_t kFieldId_libraries = 2;

    std::optional<std::vector<std::string>> modules{std::nullopt};
    std::optional<std::vector<std::string>> libraries{std::nullopt};

    auto has_modules() const -> bool {
        return modules.has_value();
    }

    auto has_libraries() const -> bool {
        return libraries.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> DependencyInfo {
        auto out = DependencyInfo{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("modules"); it != obj->end()) {
            out.modules = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.modules.reset();
        }
        if (const auto it = obj->find("libraries"); it != obj->end()) {
            out.libraries = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.libraries.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (modules.has_value()) {
            obj.emplace("modules", schema_write(*modules));
        }
        if (libraries.has_value()) {
            obj.emplace("libraries", schema_write(*libraries));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> DependencyInfo;
};

struct DependencyInfoView {
    static constexpr size_t kFieldCount = 2;

    DependencyInfoView() = default;
    explicit DependencyInfoView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit DependencyInfoView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_modules = 0;
    static constexpr size_t kSlotIndex_libraries = 1;

    auto Has_modules() const -> bool {
        return not view_.is_null(0);
    }

    auto modules() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(0);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto Has_libraries() const -> bool {
        return not view_.is_null(1);
    }

    auto libraries() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(1);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct DependencyInfoBuilder {
    static constexpr size_t kFieldCount = 2;

    DependencyInfoBuilder() : writer_(2) {}

    void set_modules(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(0, bytes); }
    void set_libraries(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(1, bytes); }

    void clear_modules() { writer_.set_null(0); }
    void clear_libraries() { writer_.set_null(1); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto DependencyInfo::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = DependencyInfoBuilder();
    if (modules.has_value()) {
        builder.set_modules(*modules);
    } else {
        builder.clear_modules();
    }
    if (libraries.has_value()) {
        builder.set_libraries(*libraries);
    } else {
        builder.clear_libraries();
    }
    return builder.Serialize();
}

inline auto DependencyInfo::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = DependencyInfoBuilder();
    if (modules.has_value()) {
        builder.set_modules(*modules);
    } else {
        builder.clear_modules();
    }
    if (libraries.has_value()) {
        builder.set_libraries(*libraries);
    } else {
        builder.clear_libraries();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto DependencyInfo::from_fixed_layout(std::span<const uint8_t> data) -> DependencyInfo {
    auto view = DependencyInfoView(data.data(), data.size());
    auto out = DependencyInfo();
    if (view.Has_modules()) { out.modules = view.modules(); }
    if (view.Has_libraries()) { out.libraries = view.libraries(); }
    return out;
}

struct ModuleConfBuilder;

struct ModuleConf {
    static constexpr uint32_t kFieldId_module = 1;
    static constexpr uint32_t kFieldId_layout = 2;
    static constexpr uint32_t kFieldId_components = 3;
    static constexpr uint32_t kFieldId_dependency = 4;

    ModuleInfo module{};
    std::optional<LayoutInfo> layout{std::nullopt};
    std::optional<std::vector<std::string>> components{std::nullopt};
    std::optional<DependencyInfo> dependency{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_layout() const -> bool {
        return layout.has_value();
    }

    auto has_components() const -> bool {
        return components.has_value();
    }

    auto has_dependency() const -> bool {
        return dependency.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ModuleConf {
        auto out = ModuleConf{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("module"); it != obj->end()) {
            out.module = schema_read<ModuleInfo>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: module");
        }
        if (const auto it = obj->find("layout"); it != obj->end()) {
            out.layout = schema_read<LayoutInfo>(it->second);
        } else {
            out.layout.reset();
        }
        if (const auto it = obj->find("components"); it != obj->end()) {
            out.components = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.components.reset();
        }
        if (const auto it = obj->find("dependency"); it != obj->end()) {
            out.dependency = schema_read<DependencyInfo>(it->second);
        } else {
            out.dependency.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("module", schema_write(module));
        if (layout.has_value()) {
            obj.emplace("layout", schema_write(*layout));
        }
        if (components.has_value()) {
            obj.emplace("components", schema_write(*components));
        }
        if (dependency.has_value()) {
            obj.emplace("dependency", schema_write(*dependency));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    static auto schema_type_id() -> star::schema::SchemaTypeId {
        return _schema_type_id();
    }

    static auto schema_descriptor() -> star::schema::SchemaDescriptor {
        return _schema_descriptor();
    }

    static void register_schema(star::schema::SchemaRegistry& registry) {
        registry.register_schema(schema_descriptor());
    }

    auto encode_binary() const -> std::vector<uint8_t> {
        return star::schema::BinaryCodec::encode(_schema_descriptor(), to_value(), unknown_fields_);
    }

    static auto from_binary(std::span<const uint8_t> bytes) -> ModuleConf {
        auto decoded = star::schema::BinaryCodec::decode_with_unknown(_schema_descriptor(), bytes);
        auto out = from_value(decoded.value);
        out.unknown_fields_ = std::move(decoded.unknown_fields);
        return out;
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return unknown_fields_;
    }

    void set_unknown_fields(star::schema::UnknownFieldSet unknown_fields) {
        unknown_fields_ = std::move(unknown_fields);
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ModuleConf;
};

struct ModuleConfView {
    static constexpr size_t kFieldCount = 4;

    ModuleConfView() = default;
    explicit ModuleConfView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ModuleConfView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ModuleConfView {
        return ModuleConfView(bytes);
    }

    static constexpr size_t kSlotIndex_module = 0;
    static constexpr size_t kSlotIndex_layout = 1;
    static constexpr size_t kSlotIndex_components = 2;
    static constexpr size_t kSlotIndex_dependency = 3;

    auto Has_module() const -> bool {
        return not view_.is_null(0);
    }

    auto module_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_layout() const -> bool {
        return not view_.is_null(1);
    }

    auto layout_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(1);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_components() const -> bool {
        return not view_.is_null(2);
    }

    auto components() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(2);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto Has_dependency() const -> bool {
        return not view_.is_null(3);
    }

    auto dependency_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(3);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ModuleConfBuilder {
    static constexpr size_t kFieldCount = 4;

    ModuleConfBuilder() : writer_(4) {}

    void set_module_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }
    void set_layout_bytes(std::span<const uint8_t> value) { writer_.set_object(1, value); }
    void set_components(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(2, bytes); }
    void set_dependency_bytes(std::span<const uint8_t> value) { writer_.set_object(3, value); }

    void clear_layout() { writer_.set_null(1); }
    void clear_components() { writer_.set_null(2); }
    void clear_dependency() { writer_.set_null(3); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto ModuleConf::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ModuleConfBuilder();
    { auto bytes = module.encode_fixed_layout(); builder.set_module_bytes(bytes); }
    if (layout.has_value()) {
        { auto bytes = layout->encode_fixed_layout(); builder.set_layout_bytes(bytes); }
    } else {
        builder.clear_layout();
    }
    if (components.has_value()) {
        builder.set_components(*components);
    } else {
        builder.clear_components();
    }
    if (dependency.has_value()) {
        { auto bytes = dependency->encode_fixed_layout(); builder.set_dependency_bytes(bytes); }
    } else {
        builder.clear_dependency();
    }
    return builder.Serialize();
}

inline auto ModuleConf::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ModuleConfBuilder();
    { auto bytes = module.encode_fixed_layout(); builder.set_module_bytes(bytes); }
    if (layout.has_value()) {
        { auto bytes = layout->encode_fixed_layout(); builder.set_layout_bytes(bytes); }
    } else {
        builder.clear_layout();
    }
    if (components.has_value()) {
        builder.set_components(*components);
    } else {
        builder.clear_components();
    }
    if (dependency.has_value()) {
        { auto bytes = dependency->encode_fixed_layout(); builder.set_dependency_bytes(bytes); }
    } else {
        builder.clear_dependency();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ModuleConf::from_fixed_layout(std::span<const uint8_t> data) -> ModuleConf {
    auto view = ModuleConfView(data.data(), data.size());
    auto out = ModuleConf();
    { auto bytes = view.module_bytes(); out.module = ModuleInfo::from_fixed_layout(bytes); }
    if (view.Has_layout()) { auto bytes = view.layout_bytes(); out.layout = LayoutInfo::from_fixed_layout(bytes); }
    if (view.Has_components()) { out.components = view.components(); }
    if (view.Has_dependency()) { auto bytes = view.dependency_bytes(); out.dependency = DependencyInfo::from_fixed_layout(bytes); }
    return out;
}

using ModuleConfSchema = ModuleConf;

}  // namespace star::generated::manifest::module

#endif // __MANIFEST_MODULE__
