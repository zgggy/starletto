#include "basement/runtime/factory/runtime_factory.h"
#include "basement/schema/binary_codec.h"
#include "basement/schema/native/schema_bootstrap.h"
#include "basement/schema/schema_validator.h"
#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/transport/api/service_bus.h"
#include "basement/transport/shm/shm_views.h"
#include "libraries/common/schema/schema_value.h"
#include "modules/setting/parameter/parameter_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>

namespace {

void register_generated_schemas(star::schema::SchemaRegistry& registry) {
    star::schema::native::register_builtin_schemas(registry);
    star::setting::schema::register_setting_schemas(registry);
}

template <typename _RequestT>
auto make_typed_request(_RequestT request_value) -> star::transport::SerializedServiceRequest {
    auto request = star::transport::ServiceRequest{};
    auto msg = std::make_shared<const _RequestT>(std::move(request_value));
    request.body.payload = star::base::TypedConstPtr::from(msg);
    request.body.schema_type_id = _RequestT::schema_type_id();
    request.body.correlation_id = star::transport::CorrelationId::generate();
    auto serializer = star::transport::MessageSerializer{};
    serializer.encode_binary = [msg]() { return msg->encode_binary(); };
    return star::transport::SerializedServiceRequest{request, serializer};
}

template <typename _ResponseT>
auto decode_typed_response(const star::transport::ServiceResponse& response) -> _ResponseT {
    if (not response.ok) {
        throw star::excep::RuntimeError("service call failed: ", response.message);
    }
    if (const auto typed = response.body.payload.as<_ResponseT>()) {
        return *typed;
    }
    if (const auto slot = response.body.payload.as<star::transport::ShmSlotView>()) {
        if (slot->size > 0) {
            return _ResponseT::from_binary(slot->bytes());
        }
    }
    if (const auto binary = response.body.payload.as<star::transport::BinaryPayloadView>()) {
        if (binary->size > 0) {
            return _ResponseT::from_binary(binary->bytes());
        }
    }
    throw star::excep::RuntimeError("typed response decode failed");
}

auto repo_root_from_cwd() -> std::filesystem::path {
    return std::filesystem::current_path();
}

auto read_text_file(const std::filesystem::path& path) -> std::string {
    std::ifstream ifs(path);
    if (not ifs.is_open()) {
        throw star::excep::RuntimeError("cannot open file: ", path.string());
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

struct CliOptions {
    std::string command{"demo"};
    std::string format{"plain"};
    std::string key;
    std::string prefix;
    std::string schema_name;
    std::string value_file;
    std::filesystem::path config_dir{};
};

struct ServiceHarness {
    star::runtime::RuntimeContextPtr context;
    star::setting::ParameterComponent component;
    std::shared_ptr<star::transport::ServiceClient> get_client;
    std::shared_ptr<star::transport::ServiceClient> set_client;
    std::shared_ptr<star::transport::ServiceClient> list_client;
};

void print_usage() {
    std::cout << "usage:\n";
    std::cout << "  parameter_service_demo demo [--format=plain|json] [--config-dir=<dir>]\n";
    std::cout << "  parameter_service_demo list [--prefix=<prefix>] [--format=plain|json] [--config-dir=<dir>]\n";
    std::cout << "  parameter_service_demo get --key=<key> [--format=plain|json] [--config-dir=<dir>]\n";
    std::cout << "  parameter_service_demo set --key=<key> --value-file=<file> [--schema=<schema>] [--format=plain|json] [--config-dir=<dir>]\n";
}

auto parse_cli(int argc, char** argv) -> CliOptions {
    auto options = CliOptions{};
    if (argc >= 2 && argv != nullptr && argv[1] != nullptr) {
        options.command = argv[1];
    }
    for (int i = 2; i < argc; ++i) {
        const auto arg = std::string(argv[i]);
        if (arg.rfind("--key=", 0) == 0) {
            options.key = arg.substr(std::string("--key=").size());
        } else if (arg.rfind("--prefix=", 0) == 0) {
            options.prefix = arg.substr(std::string("--prefix=").size());
        } else if (arg.rfind("--format=", 0) == 0) {
            options.format = arg.substr(std::string("--format=").size());
        } else if (arg.rfind("--schema=", 0) == 0) {
            options.schema_name = arg.substr(std::string("--schema=").size());
        } else if (arg.rfind("--value-file=", 0) == 0) {
            options.value_file = arg.substr(std::string("--value-file=").size());
        } else if (arg.rfind("--config-dir=", 0) == 0) {
            options.config_dir = arg.substr(std::string("--config-dir=").size());
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            std::exit(0);
        } else {
            throw star::excep::RuntimeError("unknown argument: ", arg);
        }
    }
    return options;
}

auto json_escape(const std::string& input) -> std::string {
    auto out = std::string{};
    out.reserve(input.size() + 8);
    for (const auto ch : input) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += ch;
            break;
        }
    }
    return out;
}

auto json_string(const std::string& input) -> std::string {
    return "\"" + json_escape(input) + "\"";
}

auto default_config_dir() -> std::filesystem::path {
    return repo_root_from_cwd() / "modules/setting/parameter/config";
}

auto build_harness(std::filesystem::path config_dir) -> ServiceHarness {
    if (config_dir.empty()) {
        config_dir = default_config_dir();
    }
    auto context = star::runtime::RuntimeFactory::create_native_context();
    register_generated_schemas(context->schema_registry());

    auto plan = star::generated::runtime::component_load_plan::ComponentLoadPlan{};
    plan.component_name = "parameter_demo";
    plan.component_class = "star::parameter::Parameter";
    plan.config_file_path = config_dir.string();

    auto harness = ServiceHarness{};
    harness.context = context;
    harness.component.set_context(context);
    harness.component.set_load_plan(std::move(plan));
    harness.component.init();
    harness.get_client = context->service_bus().create_client(star::transport::ServiceDescriptor{"setting.parameter.get"});
    harness.set_client = context->service_bus().create_client(star::transport::ServiceDescriptor{"setting.parameter.set"});
    harness.list_client = context->service_bus().create_client(star::transport::ServiceDescriptor{"setting.parameter.list"});
    return harness;
}

auto encode_parameter_value(
    star::runtime::RuntimeContext& context,
    const std::string& schema_name,
    const star::schema::SchemaValue& value) -> std::optional<std::string> {
    if (schema_name == "planning.motion") {
        const auto typed = star::setting::parameter::planning::MotionParameter::from_value(value);
        const auto bytes = typed.encode_binary();
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    const auto schema = context.schema_registry().find_by_name(schema_name);
    if (not schema) {
        return std::nullopt;
    }
    auto normalized = value;
    star::schema::SchemaValidator::apply_defaults(*schema, &normalized);
    star::schema::SchemaValidator::validate(*schema, normalized);
    const auto bytes = star::schema::BinaryCodec::encode(*schema, normalized);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

auto decode_parameter_value_to_text(
    star::runtime::RuntimeContext& context,
    const std::string& schema_name,
    const std::string& payload_binary) -> std::string {
    if (schema_name == "planning.motion") {
        const auto typed = star::setting::parameter::planning::MotionParameter::from_binary(
            std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(payload_binary.data()),
                payload_binary.size()));
        return star::schema::dump_schema_text(typed.to_value());
    }
    const auto schema = context.schema_registry().find_by_name(schema_name);
    if (not schema) {
        return "<unknown schema payload>";
    }
    const auto value = star::schema::BinaryCodec::decode(
        *schema,
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(payload_binary.data()),
            payload_binary.size()));
    return star::schema::dump_schema_text(value);
}

auto get_response_json(ServiceHarness& harness, const std::string& key,
    const star::setting::service::parameter::get_response::ParameterGetResponse& response) -> std::string {
    const auto schema_name = response.schema_name.value_or("");
    auto value_text = std::string{};
    if (response.payload_binary.has_value()) {
        value_text = decode_parameter_value_to_text(*harness.context, response.schema_name.value_or(key), *response.payload_binary);
    } else if (response.debug_text.has_value()) {
        value_text = *response.debug_text;
    }
    auto out = std::ostringstream{};
    out << "{"
        << "\"key\":" << json_string(response.key) << ","
        << "\"schema\":" << json_string(schema_name) << ","
        << "\"has_payload_binary\":" << (response.payload_binary.has_value() ? "true" : "false") << ","
        << "\"payload_binary_bytes\":" << (response.payload_binary.has_value() ? std::to_string(response.payload_binary->size()) : "0") << ","
        << "\"has_debug_text\":" << (response.debug_text.has_value() ? "true" : "false") << ","
        << "\"value_text\":" << json_string(value_text)
        << "}";
    return out.str();
}

void run_list(ServiceHarness& harness, const CliOptions& options) {
    auto request = star::setting::service::parameter::list_request::ParameterListRequest{};
    if (not options.prefix.empty()) {
        request.prefix = options.prefix;
    }
    const auto response =
        decode_typed_response<star::setting::service::parameter::list_response::ParameterListResponse>(
            harness.list_client->call_serialized(make_typed_request(std::move(request))));
    const auto& keys = response.keys.value_or(std::vector<std::string>{});
    if (options.format == "json") {
        std::cout << "{\"keys\":[";
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << json_string(keys[i]);
        }
        std::cout << "]}\n";
        return;
    }
    std::cout << "keys=" << keys.size() << "\n";
    for (const auto& key : keys) {
        std::cout << key << "\n";
    }
}

auto run_get(ServiceHarness& harness, const std::string& key)
    -> star::setting::service::parameter::get_response::ParameterGetResponse {
    if (key.empty()) {
        throw star::excep::RuntimeError("get requires --key");
    }
    auto request = star::setting::service::parameter::get_request::ParameterGetRequest{};
    request.key = key;
    const auto response =
        decode_typed_response<star::setting::service::parameter::get_response::ParameterGetResponse>(
            harness.get_client->call_serialized(make_typed_request(std::move(request))));
    return response;
}

void print_get_response(ServiceHarness& harness, const std::string& key, const std::string& format,
    const star::setting::service::parameter::get_response::ParameterGetResponse& response) {
    const auto schema_name = response.schema_name.value_or("");
    auto value_text = std::string{};
    if (response.payload_binary.has_value()) {
        value_text = decode_parameter_value_to_text(*harness.context, response.schema_name.value_or(key), *response.payload_binary);
    } else if (response.debug_text.has_value()) {
        value_text = *response.debug_text;
    }

    if (format == "json") {
        std::cout << get_response_json(harness, key, response) << "\n";
        return;
    }

    std::cout << "key=" << response.key << "\n";
    std::cout << "schema=" << schema_name << "\n";
    if (response.payload_binary.has_value()) {
        std::cout << "payload_binary_bytes=" << response.payload_binary->size() << "\n";
    }
    if (not value_text.empty()) {
        std::cout << value_text << "\n";
    }
}

void run_set(ServiceHarness& harness, const CliOptions& options) {
    if (options.key.empty()) {
        throw star::excep::RuntimeError("set requires --key");
    }
    if (options.value_file.empty()) {
        throw star::excep::RuntimeError("set requires --value-file");
    }
    const auto value_text = read_text_file(options.value_file);
    const auto parsed = star::schema::parse_schema_text(value_text);
    const auto schema_name = options.schema_name.empty() ? options.key : options.schema_name;

    auto request = star::setting::service::parameter::set_request::ParameterSetRequest{};
    request.key = options.key;
    request.schema_name = schema_name;
    if (const auto payload_binary = encode_parameter_value(*harness.context, schema_name, parsed); payload_binary.has_value()) {
        request.payload_binary = std::move(*payload_binary);
    } else {
        request.debug_text = star::schema::dump_schema_text(parsed);
    }

    const auto response =
        decode_typed_response<star::setting::service::parameter::set_response::ParameterSetResponse>(
            harness.set_client->call_serialized(make_typed_request(std::move(request))));
    if (not response.updated) {
        throw star::excep::RuntimeError("set response updated=false");
    }
    const auto get_response = run_get(harness, options.key);
    if (options.format == "json") {
        std::cout << "{\"updated\":true,\"after\":" << get_response_json(harness, options.key, get_response) << "}\n";
        return;
    }
    std::cout << "updated=true\n";
    print_get_response(harness, options.key, "plain", get_response);
}

void run_demo(ServiceHarness& harness, const std::string& format) {
    auto list_request = star::setting::service::parameter::list_request::ParameterListRequest{};
    list_request.prefix = std::string("planning");
    const auto list_response = decode_typed_response<star::setting::service::parameter::list_response::ParameterListResponse>(
        harness.list_client->call_serialized(make_typed_request(std::move(list_request))));
    if (not list_response.keys.has_value() || list_response.keys->empty()) {
        throw star::excep::RuntimeError("typed list response is empty");
    }

    auto get_request = star::setting::service::parameter::get_request::ParameterGetRequest{};
    get_request.key = "planning.motion";
    const auto get_before =
        decode_typed_response<star::setting::service::parameter::get_response::ParameterGetResponse>(
            harness.get_client->call_serialized(make_typed_request(get_request)));
    if (not get_before.payload_binary.has_value()) {
        throw star::excep::RuntimeError("typed get response missing payload_binary");
    }
    auto motion_before = star::setting::parameter::planning::MotionParameter::from_binary(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(get_before.payload_binary->data()),
            get_before.payload_binary->size()));

    auto updated_motion = motion_before;
    updated_motion.processor.expected_cycle_time = 200;
    if (not updated_motion.methods.has_value()) {
        updated_motion.methods = star::setting::parameter::planning::Methods{};
    }
    updated_motion.methods->trajectory_method = "piecewise_jerk";
    updated_motion.methods->enable_fallback = false;

    auto set_request = star::setting::service::parameter::set_request::ParameterSetRequest{};
    set_request.key = "planning.motion";
    set_request.schema_name = std::string("planning.motion");
    {
        const auto bytes = updated_motion.encode_binary();
        set_request.payload_binary = std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    const auto set_response =
        decode_typed_response<star::setting::service::parameter::set_response::ParameterSetResponse>(
            harness.set_client->call_serialized(make_typed_request(std::move(set_request))));
    if (not set_response.updated) {
        throw star::excep::RuntimeError("typed set response updated=false");
    }

    const auto get_after =
        decode_typed_response<star::setting::service::parameter::get_response::ParameterGetResponse>(
            harness.get_client->call_serialized(make_typed_request(get_request)));
    if (not get_after.payload_binary.has_value()) {
        throw star::excep::RuntimeError("typed get-after response missing payload_binary");
    }
    const auto motion_after = star::setting::parameter::planning::MotionParameter::from_binary(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(get_after.payload_binary->data()),
            get_after.payload_binary->size()));
    if (motion_after.processor.expected_cycle_time != 200) {
        throw star::excep::RuntimeError("typed service roundtrip mismatch: expected_cycle_time");
    }
    if (not motion_after.methods.has_value() || motion_after.methods->trajectory_method != "piecewise_jerk") {
        throw star::excep::RuntimeError("typed service roundtrip mismatch: trajectory_method");
    }
    if (motion_after.methods->enable_fallback) {
        throw star::excep::RuntimeError("typed service roundtrip mismatch: enable_fallback");
    }

    if (format == "json") {
        std::cout << "{"
                  << "\"list_ok\":true,"
                  << "\"keys\":" << list_response.keys->size() << ","
                  << "\"schema\":" << json_string(get_after.schema_name.value_or("")) << ","
                  << "\"expected_cycle_time\":" << motion_after.processor.expected_cycle_time << ","
                  << "\"trajectory_method\":" << json_string(motion_after.methods->trajectory_method) << ","
                  << "\"enable_fallback\":" << (motion_after.methods->enable_fallback ? "true" : "false")
                  << "}\n";
        return;
    }
    std::cout << "typed_service_demo=list_ok keys=" << list_response.keys->size() << "\n";
    std::cout << "typed_service_demo=get_ok schema=" << get_after.schema_name.value_or("") << "\n";
    std::cout << "typed_service_demo=set_ok expected_cycle_time=" << motion_after.processor.expected_cycle_time
              << " trajectory_method=" << motion_after.methods->trajectory_method
              << " enable_fallback=" << (motion_after.methods->enable_fallback ? "true" : "false") << "\n";
}

}  // namespace

auto main(int argc, char** argv) -> int {
    try {
        const auto options = parse_cli(argc, argv);
        if (options.format != "plain" && options.format != "json") {
            throw star::excep::RuntimeError("unsupported --format: ", options.format);
        }
        auto harness = build_harness(options.config_dir);
        if (options.command == "demo") {
            run_demo(harness, options.format);
        } else if (options.command == "list") {
            run_list(harness, options);
        } else if (options.command == "get") {
            const auto response = run_get(harness, options.key);
            print_get_response(harness, options.key, options.format, response);
        } else if (options.command == "set") {
            run_set(harness, options);
        } else {
            throw star::excep::RuntimeError("unknown command: ", options.command);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "parameter_service_demo failed: " << e.what() << "\n";
        return 1;
    }
}
