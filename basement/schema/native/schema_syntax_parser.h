#ifndef __SCHEMA_SYNTAX_PARSER_H__
#define __SCHEMA_SYNTAX_PARSER_H__

#include <cctype>
#include <cstdint>

#include "basement/core/exception.h"
#include "basement/schema/native/schema_parser_types.h"

#include <string>
#include <string_view>

namespace star::schema {

class SchemaSyntaxParser {
  public:
    explicit SchemaSyntaxParser(std::string_view input) : input_(input) {}

    auto parse() -> SchemaSyntaxDocument {
        SchemaSyntaxDocument out;
        while (true) {
            skip_ws_and_comments();
            if (eof()) {
                break;
            }
            const auto keyword = parse_identifier();
            if (keyword == "schema") {
                out.schema_name = parse_type_name();
                continue;
            }
            if (keyword == "root") {
                const auto root_name = parse_type_name();
                skip_ws_and_comments();
                if (peek() == '{') {
                    DefBlock root;
                    root.name = root_name;
                    expect('{');
                    while (true) {
                        skip_ws_and_comments();
                        if (peek() == '}') {
                            expect('}');
                            break;
                        }
                        root.fields.emplace_back(parse_field_def());
                    }
                    out.root_name = root.name;
                    out.defs.emplace_back(std::move(root));
                } else {
                    out.root_name = root_name;
                }
                continue;
            }
            if (keyword == "namespace") {
                out.namespace_name = parse_type_name();
                continue;
            }
            if (keyword == "version") {
                out.version = parse_integer();
                continue;
            }
            if (keyword == "hash") {
                out.hash = parse_uint64();
                continue;
            }
            if (keyword == "allow_unknown") {
                out.allow_unknown_fields = parse_bool();
                continue;
            }
            if (keyword == "def") {
                out.defs.emplace_back(parse_def_block());
                continue;
            }
            throw star::excep::RuntimeError("NativeSchemaLoader parse failed, unknown keyword: ", keyword);
        }
        return out;
    }

  private:
    auto eof() const -> bool { return pos_ >= input_.size(); }

    auto peek() const -> char {
        if (eof()) {
            return '\0';
        }
        return input_[pos_];
    }

    auto get() -> char {
        if (eof()) {
            return '\0';
        }
        return input_[pos_++];
    }

    void expect(char ch) {
        if (peek() != ch) {
            throw star::excep::RuntimeError("NativeSchemaLoader parse failed, expect char: ", std::string(1, ch));
        }
        ++pos_;
    }

    static auto is_name_char(char ch) -> bool {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
            return true;
        }
        switch (ch) {
        case '_':
        case '-':
        case '.':
        case ':':
        case '/':
            return true;
        default:
            return false;
        }
    }

    void skip_ws_and_comments() {
        while (not eof()) {
            const auto ch = peek();
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                ++pos_;
                continue;
            }
            if (ch == '#') {
                while (not eof() && peek() != '\n') {
                    ++pos_;
                }
                continue;
            }
            if (ch == '/' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '/') {
                pos_ += 2;
                while (not eof() && peek() != '\n') {
                    ++pos_;
                }
                continue;
            }
            break;
        }
    }

    auto parse_identifier() -> std::string {
        skip_ws_and_comments();
        if (peek() == '"') {
            return parse_string();
        }
        std::string out;
        while (not eof() && is_name_char(peek())) {
            out.push_back(get());
        }
        if (out.empty()) {
            throw star::excep::RuntimeError("NativeSchemaLoader parse failed, empty identifier");
        }
        return out;
    }

    auto parse_type_name() -> std::string {
        return parse_identifier();
    }

    auto parse_string() -> std::string {
        expect('"');
        std::string out;
        bool closed = false;
        while (not eof()) {
            const auto ch = get();
            if (ch == '"') {
                closed = true;
                break;
            }
            if (ch == '\\') {
                if (eof()) {
                    throw star::excep::RuntimeError("NativeSchemaLoader parse failed, dangling escape");
                }
                const auto esc = get();
                switch (esc) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                default:
                    out.push_back(esc);
                    break;
                }
            } else {
                out.push_back(ch);
            }
        }
        if (not closed) {
            throw star::excep::RuntimeError("NativeSchemaLoader parse failed, missing closing quote");
        }
        return out;
    }

    auto parse_integer() -> int64_t {
        const auto token = parse_identifier();
        size_t parsed = 0;
        try {
            const auto value = static_cast<int64_t>(std::stoll(token, &parsed));
            if (parsed != token.size()) {
                throw star::excep::RuntimeError("NativeSchemaLoader parse failed, invalid int token: ", token);
            }
            return value;
        } catch (...) {
            throw star::excep::RuntimeError("NativeSchemaLoader parse failed, invalid int token: ", token);
        }
    }

    auto parse_uint64() -> uint64_t {
        const auto value = parse_integer();
        if (value < 0) {
            throw star::excep::RuntimeError("NativeSchemaLoader parse failed, invalid uint64 value");
        }
        return static_cast<uint64_t>(value);
    }

    auto parse_bool() -> bool {
        const auto token = parse_identifier();
        if (token == "true") {
            return true;
        }
        if (token == "false") {
            return false;
        }
        throw star::excep::RuntimeError("NativeSchemaLoader parse failed, invalid bool token: ", token);
    }

    auto parse_type_ref() -> TypeRef {
        skip_ws_and_comments();
        TypeRef out;
        if (peek() == '[') {
            expect('[');
            out.name = parse_type_name();
            skip_ws_and_comments();
            expect(']');
            out.category = TypeRef::Category::ARRAY;
        } else if (peek() == '{') {
            expect('{');
            out.key_name = parse_type_name();
            skip_ws_and_comments();
            expect(':');
            out.value_name = parse_type_name();
            skip_ws_and_comments();
            expect('}');
            out.category = TypeRef::Category::MAP;
        } else {
            out.name = parse_type_name();
        }
        skip_ws_and_comments();
        if (peek() == '[') {
            expect('[');
            expect(']');
            out.category = TypeRef::Category::ARRAY;
        }
        skip_ws_and_comments();
        if (peek() == '?') {
            expect('?');
            out.optional = true;
        } else if (peek() == '!') {
            expect('!');
            out.explicit_required = true;
        }
        return out;
    }

    auto parse_default_value() -> star::schema::SchemaValue {
        skip_ws_and_comments();
        if (peek() == '"') {
            return star::schema::SchemaValue(parse_string());
        }
        std::string token;
        while (not eof()) {
            const auto ch = peek();
            if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '}') {
                break;
            }
            token.push_back(get());
        }
        if (token.empty()) {
            throw star::excep::RuntimeError("NativeSchemaLoader parse failed, empty default value");
        }
        const auto wrapper = std::string("value = ") + token;
        auto root = star::schema::parse_schema_text(wrapper);
        return root.as_object().at("value");
    }

    auto parse_field_def() -> FieldDef {
        FieldDef out;
        skip_ws_and_comments();
        if (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            out.id = static_cast<uint32_t>(parse_integer());
            skip_ws_and_comments();
        }
        out.name = parse_identifier();
        skip_ws_and_comments();
        expect(':');
        out.type = parse_type_ref();
        skip_ws_and_comments();
        if (peek() == '=') {
            expect('=');
            out.has_default = true;
            out.default_value = parse_default_value();
        }
        return out;
    }

    auto parse_def_block() -> DefBlock {
        DefBlock out;
        out.name = parse_type_name();
        skip_ws_and_comments();
        expect('{');
        while (true) {
            skip_ws_and_comments();
            if (peek() == '}') {
                expect('}');
                break;
            }
            out.fields.emplace_back(parse_field_def());
        }
        return out;
    }

    std::string_view input_;
    size_t pos_{0};
};

}  // namespace star::schema

#endif  // __SCHEMA_SYNTAX_PARSER_H__
