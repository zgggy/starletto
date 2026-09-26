#include <cstdint>
#include <string>

#include "basement/schema/schema_value.h"

#include "tests/support/star_test.h"

namespace {

using star::schema::SchemaObject;
using star::schema::SchemaParser;
using star::schema::SchemaValue;

auto parse(const std::string& text) -> SchemaValue { return SchemaParser{text}.parse(); }

auto string_of(const SchemaValue& value) -> std::string { return std::get<std::string>(value.as_scalar()); }

TEST_CASE("SchemaParser parses single-line fields separated by semicolons") {
    // 回归：';' 曾被吸收进值，导致 component_library 解析成 "libpid.so;"
    const auto value = parse("component { name = pid_control; type = control; component_library = libpid.so; component_class = star::control::Control }");
    const auto& component = value.as_object().at("component").as_object();

    REQUIRE_EQ(string_of(component.at("name")), std::string("pid_control"));
    REQUIRE_EQ(string_of(component.at("type")), std::string("control"));
    REQUIRE_EQ(string_of(component.at("component_library")), std::string("libpid.so"));
    REQUIRE_EQ(string_of(component.at("component_class")), std::string("star::control::Control"));
}

TEST_CASE("SchemaParser accepts a trailing semicolon before the closing brace") {
    const auto value = parse("a { x = 1; y = two; }");
    const auto& obj = value.as_object().at("a").as_object();

    REQUIRE_EQ(std::get<int64_t>(obj.at("x").as_scalar()), int64_t{1});
    REQUIRE_EQ(string_of(obj.at("y")), std::string("two"));
}

TEST_CASE("SchemaParser accepts semicolons at the top level") {
    const auto value = parse("alpha = 1; beta = two; gamma = 3.5");
    const auto& root = value.as_object();

    REQUIRE_EQ(std::get<int64_t>(root.at("alpha").as_scalar()), int64_t{1});
    REQUIRE_EQ(string_of(root.at("beta")), std::string("two"));
    REQUIRE_EQ(std::get<double>(root.at("gamma").as_scalar()), 3.5);
}

TEST_CASE("SchemaParser keeps semicolons inside quoted strings") {
    const auto value = parse("a { msg = \"libpid.so; not a separator\"; }");
    REQUIRE_EQ(string_of(value.as_object().at("a").as_object().at("msg")), std::string("libpid.so; not a separator"));
}

TEST_CASE("SchemaParser handles nested blocks and arrays") {
    const auto value = parse(R"CONF(
global {
    process_group = control
}
trigger { readers [planning_trajectory, other] }
component {
    name = pid
    priority = 10
}
)CONF");

    const auto& root = value.as_object();
    REQUIRE_EQ(string_of(root.at("global").as_object().at("process_group")), std::string("control"));
    REQUIRE_EQ(string_of(root.at("component").as_object().at("name")), std::string("pid"));

    const auto& readers = root.at("trigger").as_object().at("readers").as_array();
    REQUIRE_EQ(readers.size(), 2U);
    REQUIRE_EQ(string_of(readers[0]), std::string("planning_trajectory"));
    REQUIRE_EQ(string_of(readers[1]), std::string("other"));
}

TEST_CASE("SchemaParser preserves multi-line style without semicolons") {
    const auto value = parse("component {\n    name = motion\n    type = planning\n}\n");
    const auto& component = value.as_object().at("component").as_object();

    REQUIRE_EQ(string_of(component.at("name")), std::string("motion"));
    REQUIRE_EQ(string_of(component.at("type")), std::string("planning"));
}

TEST_CASE("SchemaParser rejects a duplicate key") {
    auto threw = false;
    try {
        (void)parse("a { x = 1; x = 2 }");
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

}  // namespace
