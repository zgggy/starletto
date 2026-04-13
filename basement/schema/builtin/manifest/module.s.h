#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_MANIFEST_MODULE_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_MANIFEST_MODULE_S_H__

#include "basement/schema/binary_codec.h"
#include "basement/schema/schema_ast.h"
#include "basement/schema/schema_registry.h"
#include "libraries/common/schema/schema_value.h"

#include <concepts>
#include <cstring>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace star::generated::manifest::module {

template <typename _ValueType>
concept SchemaStructType = requires(const star::schema::SchemaValue& value, const _ValueType& item) {
    { _ValueType::from_value(value) } -> std::same_as<_ValueType>;
    { item.to_value() } -> std::same_as<star::schema::SchemaValue>;
};

template <typename _ValueType>
struct IsStdVector : std::false_type {
};

template <typename _ItemType, typename _Allocator>
struct IsStdVector<std::vector<_ItemType, _Allocator>> : std::true_type {
    using Item = _ItemType;
};

template <typename _ValueType>
struct IsStdMap : std::false_type {
};

template <typename _MappedType, typename _Compare, typename _Allocator>
struct IsStdMap<std::map<std::string, _MappedType, _Compare, _Allocator>> : std::true_type {
    using Item = _MappedType;
};

template <typename _ValueType>
auto schema_read(const star::schema::SchemaValue& value) -> _ValueType {
    if constexpr (IsStdVector<_ValueType>::value) {
        using _Item = typename IsStdVector<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& arr = value.as_array();
        out.reserve(arr.size());
        for (const auto& item : arr) {
            out.emplace_back(schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (IsStdMap<_ValueType>::value) {
        using _Item = typename IsStdMap<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& obj = value.as_object();
        for (const auto& [key, item] : obj) {
            out.emplace(key, schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (std::is_same_v<_ValueType, std::string>) {
        return value.as<std::string>();
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, bool>) {
        return value.as<bool>();
    } else if constexpr (std::is_same_v<_ValueType, int64_t>) {
        return value.as<int64_t>();
    } else if constexpr (std::is_same_v<_ValueType, double>) {
        return value.as<double>();
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaObject>) {
        return value.as_object();
    } else if constexpr (SchemaStructType<_ValueType>) {
        return _ValueType::from_value(value);
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_read type");
    }
}

template <typename _ValueType>
auto schema_write(const _ValueType& value) -> star::schema::SchemaValue {
    if constexpr (IsStdVector<_ValueType>::value) {
        auto out = star::schema::SchemaArray{};
        out.reserve(value.size());
        for (const auto& item : value) {
            out.emplace_back(schema_write(item));
        }
        return star::schema::SchemaValue(std::move(out));
    } else if constexpr (IsStdMap<_ValueType>::value) {
        auto out = star::schema::SchemaObject{};
        for (const auto& [key, item] : value) {
            out.emplace(key, schema_write(item));
        }
        return star::schema::SchemaValue(std::move(out));
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, std::string> || std::is_same_v<_ValueType, bool> ||
                  std::is_same_v<_ValueType, int64_t> || std::is_same_v<_ValueType, double>) {
        return star::schema::schema_value_from(value);
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaObject>) {
        return star::schema::SchemaValue(value);
    } else if constexpr (SchemaStructType<_ValueType>) {
        return value.to_value();
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_write type");
    }
}

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
};

struct ModuleInfoView {
    ModuleInfo value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ModuleInfoView {
        return ModuleInfoView{ModuleInfo::from_value(value)};
    }

    auto Has_name() const -> bool {
        return true;
    }

    auto name() const -> const std::string& {
        return value.name;
    }

    auto Has_version() const -> bool {
        return value.has_version();
    }

    auto version() const -> const std::string& {
        if (not value.version.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: version");
        }
        return *value.version;
    }

    auto Has_default_backend() const -> bool {
        return value.has_default_backend();
    }

    auto default_backend() const -> const std::string& {
        if (not value.default_backend.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: default_backend");
        }
        return *value.default_backend;
    }

    auto Has_install_group() const -> bool {
        return value.has_install_group();
    }

    auto install_group() const -> const std::string& {
        if (not value.install_group.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: install_group");
        }
        return *value.install_group;
    }

};

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
};

struct LayoutInfoView {
    LayoutInfo value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> LayoutInfoView {
        return LayoutInfoView{LayoutInfo::from_value(value)};
    }

    auto Has_config_dir() const -> bool {
        return value.has_config_dir();
    }

    auto config_dir() const -> const std::string& {
        if (not value.config_dir.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: config_dir");
        }
        return *value.config_dir;
    }

    auto Has_component_dir() const -> bool {
        return value.has_component_dir();
    }

    auto component_dir() const -> const std::string& {
        if (not value.component_dir.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: component_dir");
        }
        return *value.component_dir;
    }

};

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
};

struct DependencyInfoView {
    DependencyInfo value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> DependencyInfoView {
        return DependencyInfoView{DependencyInfo::from_value(value)};
    }

    auto Has_modules() const -> bool {
        return value.has_modules();
    }

    auto modules() const -> const std::vector<std::string>& {
        if (not value.modules.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: modules");
        }
        return *value.modules;
    }

    auto Has_libraries() const -> bool {
        return value.has_libraries();
    }

    auto libraries() const -> const std::vector<std::string>& {
        if (not value.libraries.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: libraries");
        }
        return *value.libraries;
    }

};

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
};

struct ModuleConfView {
    ModuleConf value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ModuleConfView {
        return ModuleConfView{ModuleConf::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ModuleConfView {
        return ModuleConfView{ModuleConf::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_module() const -> bool {
        return true;
    }

    auto module() const -> const ModuleInfo& {
        return value.module;
    }

    auto Has_layout() const -> bool {
        return value.has_layout();
    }

    auto layout() const -> const LayoutInfo& {
        if (not value.layout.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: layout");
        }
        return *value.layout;
    }

    auto Has_components() const -> bool {
        return value.has_components();
    }

    auto components() const -> const std::vector<std::string>& {
        if (not value.components.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: components");
        }
        return *value.components;
    }

    auto Has_dependency() const -> bool {
        return value.has_dependency();
    }

    auto dependency() const -> const DependencyInfo& {
        if (not value.dependency.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: dependency");
        }
        return *value.dependency;
    }

};

struct ModuleConfBuilder {
    ModuleConf value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const ModuleConf& value) -> ModuleConfBuilder {
        return ModuleConfBuilder{value, value.unknown_fields()};
    }

    void set_unknown_fields(star::schema::UnknownFieldSet value_in) {
        unknown_fields = std::move(value_in);
    }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        if (buf == nullptr) {
            return 0;
        }
        auto tmp = value;
        tmp.set_unknown_fields(unknown_fields);
        const auto bytes = tmp.encode_binary();
        if (bytes.size() > cap) {
            return 0;
        }
        if (not bytes.empty()) {
            std::memcpy(buf, bytes.data(), bytes.size());
        }
        return bytes.size();
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }
};

using ModuleConfSchema = ModuleConf;

}  // namespace star::generated::manifest::module

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_MANIFEST_MODULE_S_H__
