#include "basement/schema/binary_codec.h"
#include "basement/schema/native/native_schema_loader.h"
#include "basement/schema/schema_validator.h"
#include "libraries/common/schema/schema_value.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

auto read_text_file(const std::string& path) -> std::string {
    std::ifstream ifs(path);
    if (not ifs.is_open()) {
        throw std::runtime_error("cannot open file: " + path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

}  // namespace

auto main(int argc, char** argv) -> int {
    try {
        if (argc < 3 || argv == nullptr || argv[1] == nullptr || argv[2] == nullptr) {
            std::cerr << "usage: schema_codec_demo <schema> <value-file>\n";
            return 2;
        }

        const auto schema_path = std::string(argv[1]);
        const auto value_path = std::string(argv[2]);

        const auto schema = star::schema::NativeSchemaLoader().load_file(schema_path);
        auto value = star::schema::parse_schema_text(read_text_file(value_path));
        star::schema::SchemaValidator::validate(schema, &value);

        const auto bytes = star::schema::BinaryCodec::encode(schema, value);
        auto decoded = star::schema::BinaryCodec::decode(schema, bytes);
        star::schema::SchemaValidator::validate(schema, &decoded);

        const auto before = star::schema::dump_schema_text(value);
        const auto after = star::schema::dump_schema_text(decoded);
        if (before != after) {
            std::cerr << "codec roundtrip mismatch\n";
            return 1;
        }

        std::cout << "schema=" << schema.type_id.name << "\n";
        std::cout << "bytes=" << bytes.size() << "\n";
        std::cout << "roundtrip=ok\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "schema_codec_demo failed: " << e.what() << "\n";
        return 1;
    }
}
