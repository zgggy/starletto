#ifndef __CONFIG_COMPONENT_H__
#define __CONFIG_COMPONENT_H__

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/schema/binary_codec.h"
#include "basement/schema/schema_registry.h"
#include "basement/schema/schema_validator.h"
#include "basement/transport/api/message_bus.h"
#include "libraries/common/schema/schema_value.h"
#include "modules/setting/schema/config_snapshot.s.h"

namespace star::setting {

class ConfigComponent : public star::runtime::Component {
  public:
    void init() override {
        if (not context()) {
            throw star::excep::RuntimeError("ConfigComponent init failed, context is null");
        }
        snapshot_ = std::make_shared<const star::generated::setting::config::snapshot::ConfigSnapshot>(load_snapshot());

        auto writer_descriptor = star::transport::ChannelDescriptor{};
        writer_descriptor.name = resolve_snapshot_channel_name();
        writer_descriptor.type_name = resolve_snapshot_type_name();
        writer_descriptor.schema_type_id = default_snapshot_schema_type_id();
        writer_ = context()->message_bus().create_writer(writer_descriptor);
    }

    auto process_once() -> bool override {
        write_snapshot();
        return static_cast<bool>(snapshot_);
    }

  private:
    static auto default_snapshot_channel_name() -> std::string { return "setting.config.snapshot"; }

    static auto default_snapshot_type_name() -> std::string { return "star::generated::setting::config::snapshot::ConfigSnapshot"; }

    static auto default_snapshot_schema_type_id() -> star::schema::SchemaTypeId {
        star::schema::SchemaTypeId out;
        out.name = "star.setting.config.snapshot";
        out.version = 1;
        return out;
    }

    auto resolve_snapshot_channel_name() const -> std::string {
        const auto* plan = load_plan();
        if (plan && plan->outputs.has_value()) {
            for (const auto& e : *plan->outputs) {
                if (e.name == "snapshot" && e.channel.has_value() && !e.channel->empty()) {
                    return *e.channel;
                }
            }
        }
        return default_snapshot_channel_name();
    }

    auto resolve_snapshot_type_name() const -> std::string {
        const auto* plan = load_plan();
        if (plan && plan->outputs.has_value()) {
            for (const auto& e : *plan->outputs) {
                if (e.name == "snapshot" && e.type_name.has_value() && !e.type_name->empty()) {
                    return *e.type_name;
                }
            }
        }
        return default_snapshot_type_name();
    }

    static auto replace_all(std::string s, char from, char to) -> std::string {
        for (auto& ch : s) {
            if (ch == from) {
                ch = to;
            }
        }
        return s;
    }

    static auto strip_extension(const std::string& path) -> std::string {
        const auto pos = path.rfind('.');
        if (pos == std::string::npos) {
            return path;
        }
        return path.substr(0, pos);
    }

    static auto is_conf_file(const std::filesystem::path& p) -> bool {
        if (not std::filesystem::is_regular_file(p)) {
            return false;
        }
        return p.extension() == ".conf";
    }

    auto resolve_config_root_path() const -> std::filesystem::path {
        const auto* plan = load_plan();
        if (plan && plan->config_file_path.has_value() && !plan->config_file_path->empty()) {
            const auto raw = std::filesystem::path(*plan->config_file_path);
            if (raw.is_absolute() && std::filesystem::exists(raw)) {
                return raw;
            }
            std::vector<std::filesystem::path> candidates;
            if (plan->module_config_dir.has_value() && !plan->module_config_dir->empty()) {
                candidates.emplace_back(std::filesystem::path(*plan->module_config_dir) / raw);
            }
            if (plan->module_dir.has_value() && !plan->module_dir->empty()) {
                candidates.emplace_back(std::filesystem::path(*plan->module_dir) / raw);
            }
            if (plan->component_conf_path.has_value() && !plan->component_conf_path->empty()) {
                candidates.emplace_back(std::filesystem::path(*plan->component_conf_path).parent_path() / raw);
            }
            candidates.emplace_back(raw);
            for (const auto& candidate : candidates) {
                if (std::filesystem::exists(candidate)) {
                    return std::filesystem::absolute(candidate);
                }
            }
            return std::filesystem::absolute(candidates.front());
        }
        return {};
    }

    auto load_snapshot() const -> star::generated::setting::config::snapshot::ConfigSnapshot {
        const auto root = resolve_config_root_path();
        if (not std::filesystem::exists(root)) {
            throw star::excep::RuntimeError("ConfigComponent load config failed, path not exists: ", root.string());
        }

        std::map<std::string, star::generated::setting::config::snapshot::SnapshotEntry> entries;
        size_t loaded = 0;
        size_t binary_only_entries = 0;
        size_t debug_text_only_entries = 0;
        if (std::filesystem::is_directory(root)) {
            for (const auto& it : std::filesystem::recursive_directory_iterator(root)) {
                if (not is_conf_file(it.path())) {
                    continue;
                }
                const auto rel = std::filesystem::relative(it.path(), root).generic_string();
                auto key = replace_all(strip_extension(rel), '/', '.');
                key = replace_all(std::move(key), '\\', '.');
                auto value = star::schema::parse_schema_file(it.path().string());
                normalize_entry_if_schema_exists(key, &value);
                auto entry = build_snapshot_entry(key, value);
                if (entry.payload_binary.has_value()) {
                    ++binary_only_entries;
                } else if (entry.debug_text.has_value()) {
                    ++debug_text_only_entries;
                }
                entries.emplace(key, std::move(entry));
                ++loaded;
            }
        } else {
            auto key = strip_extension(root.filename().string());
            auto value = star::schema::parse_schema_file(root.string());
            normalize_entry_if_schema_exists(key, &value);
            auto entry = build_snapshot_entry(key, value);
            if (entry.payload_binary.has_value()) {
                ++binary_only_entries;
            } else if (entry.debug_text.has_value()) {
                ++debug_text_only_entries;
            }
            entries.emplace(key, std::move(entry));
            ++loaded;
        }

        logI << "ConfigComponent loaded " << loaded << " config(s) from: " << root.string();
        logI << "ConfigComponent snapshot entries: binary_only=" << binary_only_entries << ", debug_text_only=" << debug_text_only_entries;
        auto out = star::generated::setting::config::snapshot::ConfigSnapshot{};
        out.entries = std::move(entries);
        auto value = out.to_value();
        const auto schema = context()->schema_registry().find_by_name(default_snapshot_schema_type_id().name);
        if (schema) {
            star::schema::SchemaValidator::apply_defaults(*schema, &value);
            star::schema::SchemaValidator::validate(*schema, value);
        }
        return out;
    }

    void normalize_entry_if_schema_exists(const std::string& key, star::schema::SchemaValue* value) const {
        if (value == nullptr) {
            return;
        }
        const auto schema = context()->schema_registry().find_by_name(key);
        if (not schema) {
            return;
        }
        star::schema::SchemaValidator::apply_defaults(*schema, value);
        star::schema::SchemaValidator::validate(*schema, *value);
        const auto bytes = star::schema::BinaryCodec::encode(*schema, *value);
        *value = star::schema::BinaryCodec::decode(*schema, std::span<const uint8_t>(bytes.data(), bytes.size()));
    }

    auto build_snapshot_entry(const std::string& key, const star::schema::SchemaValue& value) const -> star::generated::setting::config::snapshot::SnapshotEntry {
        auto entry = star::generated::setting::config::snapshot::SnapshotEntry{};
        entry.schema_name = key;
        if (const auto payload_binary = encode_schema_known_entry(key, value); payload_binary.has_value()) {
            entry.payload_binary = std::move(*payload_binary);
            return entry;
        }
        entry.debug_text = star::schema::dump_schema_text(value);
        return entry;
    }

    auto encode_schema_known_entry(const std::string& key, const star::schema::SchemaValue& value) const -> std::optional<std::string> {
        const auto schema = context()->schema_registry().find_by_name(key);
        if (not schema) {
            return std::nullopt;
        }
        const auto bytes = star::schema::BinaryCodec::encode(*schema, value);
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    void write_snapshot() {
        if (not writer_ || not snapshot_) {
            return;
        }
        auto view = star::transport::MessageView{};
        view.payload = star::base::TypedConstPtr::from(snapshot_);
        view.schema_type_id = default_snapshot_schema_type_id();
        auto serializer = star::transport::MessageSerializer{};
        serializer.encode_binary = [snapshot = snapshot_]() { return snapshot->encode_binary(); };
        const auto r = writer_->write_serialized(star::transport::SerializedMessage{view, serializer});
        if (!r.ok()) {
            throw star::excep::RuntimeError("ConfigComponent write_snapshot failed: status=", star::transport::write_status_string(r.status), ", err=", r.error_message);
        }
    }

    std::shared_ptr<const star::generated::setting::config::snapshot::ConfigSnapshot> snapshot_;
    std::shared_ptr<star::transport::Writer> writer_;
};

inline auto register_native_config_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<ConfigComponent>("star::setting::ConfigComponent");
    registry.register_component<ConfigComponent>("star::setting::Config");
}

}  // namespace star::setting

#endif  // __CONFIG_COMPONENT_H__
