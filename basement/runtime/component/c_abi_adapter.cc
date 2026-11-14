#include "basement/runtime/component/c_abi_adapter.h"

#include <cstring>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>

#include "basement/core/logger/logger.h"

namespace star::runtime {

namespace {

auto validate_component_descriptor(const star_plugin_component_v1& component, size_t index) -> void {
    if (component.class_name == nullptr || component.class_name[0] == '\0') {
        throw star::excep::RuntimeError("C ABI plugin invalid component descriptor: empty class_name at index ", index);
    }
    if (component.create == nullptr) {
        throw star::excep::RuntimeError("C ABI plugin invalid component descriptor: create is null, class=", component.class_name);
    }
    if (component.destroy == nullptr) {
        throw star::excep::RuntimeError("C ABI plugin invalid component descriptor: destroy is null, class=", component.class_name);
    }
}

}  // namespace

auto make_c_abi_component(const star_plugin_component_v1* component) -> std::unique_ptr<Component> {
    return std::make_unique<CAbiComponentAdapter>(component);
}

void register_c_abi_plugin_components(const star_plugin_v1* plugin, ComponentRegistry& registry) {
    if (plugin == nullptr) {
        throw star::excep::RuntimeError("C ABI plugin entry returned null descriptor");
    }
    if (plugin->abi_version != STAR_PLUGIN_ABI_VERSION) {
        throw star::excep::RuntimeError("C ABI plugin version mismatch: plugin=", plugin->abi_version, ", host=", STAR_PLUGIN_ABI_VERSION);
    }
    if (plugin->component_count > 0 && plugin->components == nullptr) {
        throw star::excep::RuntimeError("C ABI plugin descriptor has component_count>0 but components=null");
    }

    auto seen = std::unordered_set<std::string>{};
    for (size_t i = 0; i < plugin->component_count; ++i) {
        const auto& component = plugin->components[i];
        validate_component_descriptor(component, i);
        const auto class_name = std::string(component.class_name);
        if (not seen.insert(class_name).second) {
            throw star::excep::RuntimeError("C ABI plugin registers duplicate component_class: ", class_name);
        }
        registry.register_factory(class_name, [descriptor = &component]() { return make_c_abi_component(descriptor); });
    }
}

CAbiComponentAdapter::CAbiComponentAdapter(const star_plugin_component_v1* component) : component_(component) {
    if (component_ == nullptr) {
        throw star::excep::RuntimeError("CAbiComponentAdapter created with null component descriptor");
    }
    validate_component_descriptor(*component_, 0);
    self_ = component_->create();
    if (self_ == nullptr) {
        throw star::excep::RuntimeError("C ABI component create returned null: class=", component_->class_name);
    }
    api_.abi_version = STAR_PLUGIN_ABI_VERSION;
    api_.ctx = this;
    api_.read_latest = &CAbiComponentAdapter::read_latest_thunk;
    api_.publish = &CAbiComponentAdapter::publish_thunk;
    api_.last_error = &CAbiComponentAdapter::last_error_thunk;
}

CAbiComponentAdapter::~CAbiComponentAdapter() {
    if (self_ != nullptr && component_ != nullptr && component_->destroy != nullptr) {
        // C 回调不允许抛异常；这里到 C ABI 边界为止
        component_->destroy(self_);
        self_ = nullptr;
    }
}

void CAbiComponentAdapter::init() {
    if (initialized_) {
        return;
    }
    last_error_.clear();
    if (component_->init != nullptr) {
        const auto rc = component_->init(self_, &api_);
        if (rc != 0) {
            auto message = std::string("C ABI component init failed: class=") + component_->class_name + ", rc=" + std::to_string(rc);
            if (not last_error_.empty()) {
                message += ", error=" + last_error_;
            }
            throw star::excep::RuntimeError(message);
        }
    }
    initialized_ = true;
}

auto CAbiComponentAdapter::process_once() -> bool {
    if (component_->process == nullptr) {
        return true;
    }
    if (const auto ctx = context()) {
        const auto& frame = ctx->current_frame();
        api_.frame_id = frame.frame_id;
        api_.timestamp_ns = frame.monotonic_time_ns;
    }
    last_error_.clear();
    const auto rc = component_->process(self_);
    if (rc != 0) {
        if (last_error_.empty()) {
            last_error_ = "component process returned " + std::to_string(rc);
        }
        logE << "C ABI component process failed: class=" << component_->class_name << ", rc=" << rc << ", error=" << last_error_;
        return false;
    }
    return true;
}

void CAbiComponentAdapter::shutdown() {
    if (shutdown_called_) {
        return;
    }
    shutdown_called_ = true;
    if (component_->shutdown != nullptr) {
        component_->shutdown(self_);
    }
}

auto CAbiComponentAdapter::read_latest_thunk(void* ctx, const char* port, void* buffer, size_t capacity, size_t* out_size, const char** schema_name, int64_t* schema_version,
                                             uint64_t* schema_hash) -> int {
    if (ctx == nullptr) {
        return -1;
    }
    return static_cast<CAbiComponentAdapter*>(ctx)->read_latest(port, buffer, capacity, out_size, schema_name, schema_version, schema_hash);
}

auto CAbiComponentAdapter::publish_thunk(void* ctx, const char* port, const void* data, size_t size, const char* schema_name, int64_t schema_version, uint64_t schema_hash) -> int {
    if (ctx == nullptr) {
        return -1;
    }
    return static_cast<CAbiComponentAdapter*>(ctx)->publish(port, data, size, schema_name, schema_version, schema_hash);
}

auto CAbiComponentAdapter::last_error_thunk(void* ctx) -> const char* {
    if (ctx == nullptr) {
        return "";
    }
    return static_cast<CAbiComponentAdapter*>(ctx)->last_error_.c_str();
}

auto CAbiComponentAdapter::read_latest(const char* port, void* buffer, size_t capacity, size_t* out_size, const char** schema_name, int64_t* schema_version,
                                       uint64_t* schema_hash) -> int {
    if (port == nullptr || out_size == nullptr) {
        set_error("read_latest: port/out_size must not be null");
        return -1;
    }
    try {
        const auto raw = read_latest_raw(port);
        if (not raw.has_value()) {
            return 1;
        }
        last_schema_name_ = raw->schema_type_id.name;
        *out_size = raw->bytes.size();
        if (schema_name != nullptr) {
            *schema_name = last_schema_name_.empty() ? nullptr : last_schema_name_.c_str();
        }
        if (schema_version != nullptr) {
            *schema_version = raw->schema_type_id.version;
        }
        if (schema_hash != nullptr) {
            *schema_hash = raw->schema_type_id.hash;
        }
        if (raw->bytes.empty()) {
            last_error_.clear();
            return 0;
        }
        if (buffer == nullptr || capacity < raw->bytes.size()) {
            set_error("read_latest: buffer too small for port " + std::string(port));
            return 2;
        }
        std::memcpy(buffer, raw->bytes.data(), raw->bytes.size());
        last_error_.clear();
        return 0;
    } catch (const std::exception& e) {
        set_error(std::string("read_latest failed: ") + e.what());
        return -1;
    } catch (...) {
        set_error("read_latest failed: unknown exception");
        return -1;
    }
}

auto CAbiComponentAdapter::publish(const char* port, const void* data, size_t size, const char* schema_name, int64_t schema_version, uint64_t schema_hash) -> int {
    if (port == nullptr) {
        set_error("publish: port must not be null");
        return -1;
    }
    if (data == nullptr && size > 0) {
        set_error("publish: data must not be null when size>0");
        return -1;
    }
    try {
        auto type_id = star::schema::SchemaTypeId{};
        if (schema_name != nullptr) {
            type_id.name = schema_name;
        }
        type_id.version = schema_version;
        type_id.hash = schema_hash;

        auto bytes = std::span<const uint8_t>{};
        if (size > 0) {
            bytes = std::span<const uint8_t>(static_cast<const uint8_t*>(data), size);
        }
        const auto result = write_raw(port, bytes, type_id);
        if (not result.ok()) {
            auto message = std::string("publish failed: port=") + port + ", status=" + star::transport::write_status_string(result.status);
            if (not result.error_message.empty()) {
                message += ", error=" + result.error_message;
            }
            set_error(std::move(message));
            return -1;
        }
        last_error_.clear();
        return 0;
    } catch (const std::exception& e) {
        set_error(std::string("publish failed: ") + e.what());
        return -1;
    } catch (...) {
        set_error("publish failed: unknown exception");
        return -1;
    }
}

void CAbiComponentAdapter::set_error(std::string message) {
    last_error_ = std::move(message);
}

}  // namespace star::runtime

extern "C" uint32_t star_runtime_plugin_abi_version(void) {
    return STAR_PLUGIN_ABI_VERSION;
}
