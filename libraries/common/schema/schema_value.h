#ifndef __SCHEMA_VALUE_H__
#define __SCHEMA_VALUE_H__

#include "basement/core/exception.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace star::schema {

class SchemaValue;

using SchemaObject = std::map<std::string, SchemaValue>;
using SchemaArray = std::vector<SchemaValue>;

template <typename _ValueType>
auto schema_value_cast(const SchemaValue& value) -> _ValueType;

template <typename _ValueType>
auto schema_value_from(const _ValueType& value) -> SchemaValue;

auto find_value(const SchemaObject& obj, const std::string& dotted_key) -> const SchemaValue*;

class SchemaValue {
public:
    using Scalar = std::variant<std::string, int64_t, double, bool, std::nullptr_t>;
    using Storage = std::variant<Scalar, SchemaArray, SchemaObject>;

    SchemaValue() : data_(Scalar{nullptr}) {}
    explicit SchemaValue(const std::string& v) : data_(Scalar{v}) {}
    explicit SchemaValue(std::string&& v) : data_(Scalar{std::move(v)}) {}
    explicit SchemaValue(int64_t v) : data_(Scalar{v}) {}
    explicit SchemaValue(double v) : data_(Scalar{v}) {}
    explicit SchemaValue(bool v) : data_(Scalar{v}) {}
    explicit SchemaValue(std::nullptr_t) : data_(Scalar{nullptr}) {}
    explicit SchemaValue(const SchemaArray& v) : data_(v) {}
    explicit SchemaValue(SchemaArray&& v) : data_(std::move(v)) {}
    explicit SchemaValue(const SchemaObject& v) : data_(v) {}
    explicit SchemaValue(SchemaObject&& v) : data_(std::move(v)) {}

    auto is_scalar() const -> bool { return std::holds_alternative<Scalar>(data_); }
    auto is_array() const -> bool { return std::holds_alternative<SchemaArray>(data_); }
    auto is_object() const -> bool { return std::holds_alternative<SchemaObject>(data_); }

    auto as_scalar() const -> const Scalar& { return std::get<Scalar>(data_); }
    auto as_array() const -> const SchemaArray& { return std::get<SchemaArray>(data_); }
    auto as_object() const -> const SchemaObject& { return std::get<SchemaObject>(data_); }

    auto as_array() -> SchemaArray& { return std::get<SchemaArray>(data_); }
    auto as_object() -> SchemaObject& { return std::get<SchemaObject>(data_); }

    auto get_object_ptr() const -> const SchemaObject* {
        if (not is_object()) {
            return nullptr;
        }
        return &std::get<SchemaObject>(data_);
    }

    auto get_array_ptr() const -> const SchemaArray* {
        if (not is_array()) {
            return nullptr;
        }
        return &std::get<SchemaArray>(data_);
    }

    auto get_scalar_ptr() const -> const Scalar* {
        if (not is_scalar()) {
            return nullptr;
        }
        return &std::get<Scalar>(data_);
    }

    auto size() const -> size_t {
        if (is_array()) {
            return as_array().size();
        }
        if (is_object()) {
            return as_object().size();
        }
        throw star::excep::RuntimeError("SchemaValue size failed, value is not array or object");
    }

    auto empty() const -> bool {
        return size() == 0;
    }

    auto has(const std::string& key) const -> bool {
        const auto* obj = get_object_ptr();
        if (obj == nullptr) {
            return false;
        }
        return obj->find(key) != obj->end();
    }

    auto find(const std::string& dotted_key) const -> const SchemaValue* {
        const auto* obj = get_object_ptr();
        if (obj == nullptr) {
            return nullptr;
        }
        return find_value(*obj, dotted_key);
    }

    auto at(const std::string& key) const -> const SchemaValue& {
        const auto* obj = get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("SchemaValue at failed, value is not object");
        }
        const auto it = obj->find(key);
        if (it == obj->end()) {
            throw star::excep::RuntimeError("SchemaValue at failed, missing key: ", key);
        }
        return it->second;
    }

    auto at(const std::string& key) -> SchemaValue& {
        auto* obj = get_object_ptr_mutable();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("SchemaValue at failed, value is not object");
        }
        const auto it = obj->find(key);
        if (it == obj->end()) {
            throw star::excep::RuntimeError("SchemaValue at failed, missing key: ", key);
        }
        return it->second;
    }

    auto at(size_t index) const -> const SchemaValue& {
        const auto* arr = get_array_ptr();
        if (arr == nullptr) {
            throw star::excep::RuntimeError("SchemaValue at failed, value is not array");
        }
        if (index >= arr->size()) {
            throw star::excep::RuntimeError("SchemaValue at failed, array index out of range");
        }
        return (*arr)[index];
    }

    auto at(size_t index) -> SchemaValue& {
        auto* arr = get_array_ptr_mutable();
        if (arr == nullptr) {
            throw star::excep::RuntimeError("SchemaValue at failed, value is not array");
        }
        if (index >= arr->size()) {
            throw star::excep::RuntimeError("SchemaValue at failed, array index out of range");
        }
        return (*arr)[index];
    }

    template <typename _ValueType>
    auto as() const -> _ValueType {
        return schema_value_cast<_ValueType>(*this);
    }

    template <typename _ValueType>
    auto to_vector() const -> std::vector<_ValueType> {
        const auto* arr = get_array_ptr();
        if (arr == nullptr) {
            throw star::excep::RuntimeError("SchemaValue to_vector failed, value is not array");
        }
        auto out = std::vector<_ValueType>{};
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(schema_value_cast<_ValueType>(item));
        }
        return out;
    }

    template <typename _ValueType>
    auto to_map() const -> std::map<std::string, _ValueType> {
        const auto* obj = get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("SchemaValue to_map failed, value is not object");
        }
        auto out = std::map<std::string, _ValueType>{};
        for (const auto& [key, item] : *obj) {
            out.emplace(key, schema_value_cast<_ValueType>(item));
        }
        return out;
    }

    template <typename _ValueType>
    static auto from_vector(const std::vector<_ValueType>& value) -> SchemaValue {
        return schema_value_from(value);
    }

    template <typename _ValueType>
    static auto from_map(const std::map<std::string, _ValueType>& value) -> SchemaValue {
        return schema_value_from(value);
    }

private:
    auto get_object_ptr_mutable() -> SchemaObject* {
        if (not is_object()) {
            return nullptr;
        }
        return &std::get<SchemaObject>(data_);
    }

    auto get_array_ptr_mutable() -> SchemaArray* {
        if (not is_array()) {
            return nullptr;
        }
        return &std::get<SchemaArray>(data_);
    }

    Storage data_;
};

class SchemaParser {
public:
    explicit SchemaParser(std::string_view input) : input_(input) {}

    auto parse() -> SchemaValue {
        skip_ws_and_comments();
        SchemaObject root;
        while (not eof()) {
            const auto key = parse_key();
            auto val = parse_field_value();
            insert_field(&root, key, std::move(val));
            skip_ws_and_comments();
        }
        return SchemaValue(std::move(root));
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
            throw star::excep::RuntimeError("SchemaParser expect failed, expect char: ", std::string(1, ch));
        }
        ++pos_;
    }

    static auto is_ident_char(char c) -> bool {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            return true;
        }
        switch (c) {
        case '_':
        case '-':
        case '.':
        case '/':
            return true;
        default:
            return false;
        }
    }

    void skip_ws_and_comments() {
        while (not eof()) {
            const auto c = peek();
            if (std::isspace(static_cast<unsigned char>(c)) != 0) {
                ++pos_;
                continue;
            }
            if (c == '#') {
                while (not eof() && peek() != '\n') {
                    ++pos_;
                }
                continue;
            }
            if (c == '/' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '/') {
                pos_ += 2;
                while (not eof() && peek() != '\n') {
                    ++pos_;
                }
                continue;
            }
            break;
        }
    }

    auto parse_key() -> std::string {
        skip_ws_and_comments();
        if (peek() == '"') {
            return parse_string();
        }
        std::string out;
        while (not eof() && is_ident_char(peek())) {
            out.push_back(get());
        }
        if (out.empty()) {
            throw star::excep::RuntimeError("SchemaParser parse_key failed, empty key");
        }
        return out;
    }

    auto parse_string() -> std::string {
        expect('"');
        std::string out;
        bool closed = false;
        while (not eof()) {
            const auto c = get();
            if (c == '"') {
                closed = true;
                break;
            }
            if (c == '\\') {
                if (eof()) {
                    throw star::excep::RuntimeError("SchemaParser parse_string failed, dangling escape");
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
                out.push_back(c);
            }
        }
        if (not closed) {
            throw star::excep::RuntimeError("SchemaParser parse_string failed, missing closing quote");
        }
        return out;
    }

    auto parse_number_or_ident_value() -> SchemaValue {
        std::string token;
        while (not eof()) {
            const auto c = peek();
            if (std::isspace(static_cast<unsigned char>(c)) != 0 || c == ',' || c == ']' || c == '}') {
                break;
            }
            token.push_back(get());
        }
        if (token.empty()) {
            throw star::excep::RuntimeError("SchemaParser parse_value failed, empty token");
        }
        if (token == "true") {
            return SchemaValue(true);
        }
        if (token == "false") {
            return SchemaValue(false);
        }
        if (token == "null") {
            return SchemaValue(nullptr);
        }

        bool has_dot = false;
        bool has_exp = false;
        for (const auto ch : token) {
            if (ch == '.') {
                has_dot = true;
            }
            if (ch == 'e' || ch == 'E') {
                has_exp = true;
            }
        }
        if (has_dot || has_exp) {
            try {
                size_t parsed = 0;
                const auto v = std::stod(token, &parsed);
                if (parsed == token.size()) {
                    return SchemaValue(v);
                }
            } catch (...) {
            }
        } else {
            try {
                size_t parsed = 0;
                const auto v = static_cast<int64_t>(std::stoll(token, &parsed));
                if (parsed == token.size()) {
                    return SchemaValue(v);
                }
            } catch (...) {
            }
        }

        return SchemaValue(token);
    }

    auto parse_array() -> SchemaValue {
        expect('[');
        skip_ws_and_comments();
        SchemaArray out;
        if (peek() == ']') {
            expect(']');
            return SchemaValue(std::move(out));
        }
        while (not eof()) {
            auto v = parse_value();
            out.emplace_back(std::move(v));
            skip_ws_and_comments();
            if (peek() == ',') {
                expect(',');
                skip_ws_and_comments();
                continue;
            }
            if (peek() == ']') {
                expect(']');
                break;
            }
            throw star::excep::RuntimeError("SchemaParser parse_array failed, expect ',' or ']'");
        }
        return SchemaValue(std::move(out));
    }

    auto parse_object() -> SchemaValue {
        expect('{');
        skip_ws_and_comments();
        SchemaObject obj;
        while (not eof()) {
            skip_ws_and_comments();
            if (peek() == '}') {
                expect('}');
                break;
            }
            const auto key = parse_key();
            auto val = parse_field_value();
            insert_field(&obj, key, std::move(val));
            skip_ws_and_comments();
        }
        return SchemaValue(std::move(obj));
    }

    auto parse_value() -> SchemaValue {
        skip_ws_and_comments();
        const auto c = peek();
        if (c == '"') {
            return SchemaValue(parse_string());
        }
        if (c == '{') {
            return parse_object();
        }
        if (c == '[') {
            return parse_array();
        }
        return parse_number_or_ident_value();
    }

    auto parse_field_value() -> SchemaValue {
        skip_ws_and_comments();
        const auto c = peek();
        if (c == '{' || c == '[') {
            return parse_value();
        }
        expect('=');
        skip_ws_and_comments();
        return parse_value();
    }

    static void insert_field(SchemaObject* obj, const std::string& key, SchemaValue value) {
        if (obj == nullptr) {
            throw star::excep::RuntimeError("SchemaParser insert_field failed, object is null");
        }
        const auto [_, inserted] = obj->emplace(key, std::move(value));
        if (not inserted) {
            throw star::excep::RuntimeError("SchemaParser duplicate key: ", key);
        }
    }

    std::string_view input_;
    size_t pos_{0};
};

inline auto parse_schema_text(std::string_view schema_text) -> SchemaValue {
    return SchemaParser(schema_text).parse();
}

inline auto parse_schema_file(const std::string& path) -> SchemaValue {
    std::ifstream ifs(path);
    if (not ifs.is_open()) {
        throw star::excep::RuntimeError("parse_schema_file failed, cannot open file: ", path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return parse_schema_text(oss.str());
}

template <typename _ValueType>
inline auto schema_value_cast(const SchemaValue& value) -> _ValueType {
    if constexpr (std::is_same_v<_ValueType, SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, std::string>) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<std::string>(*scalar)) {
            throw star::excep::RuntimeError("schema_value_cast failed, value is not string");
        }
        return std::get<std::string>(*scalar);
    } else if constexpr (std::is_same_v<_ValueType, bool>) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<bool>(*scalar)) {
            throw star::excep::RuntimeError("schema_value_cast failed, value is not bool");
        }
        return std::get<bool>(*scalar);
    } else if constexpr (std::is_same_v<_ValueType, double>) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr) {
            throw star::excep::RuntimeError("schema_value_cast failed, value is not numeric");
        }
        if (std::holds_alternative<double>(*scalar)) {
            return std::get<double>(*scalar);
        }
        if (std::holds_alternative<int64_t>(*scalar)) {
            return static_cast<double>(std::get<int64_t>(*scalar));
        }
        throw star::excep::RuntimeError("schema_value_cast failed, value is not numeric");
    } else if constexpr (std::is_integral_v<_ValueType> && not std::is_same_v<_ValueType, bool>) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr) {
            throw star::excep::RuntimeError("schema_value_cast failed, value is not integer");
        }
        if (std::holds_alternative<int64_t>(*scalar)) {
            return static_cast<_ValueType>(std::get<int64_t>(*scalar));
        }
        throw star::excep::RuntimeError("schema_value_cast failed, value is not integer");
    } else {
        static_assert(
            std::is_same_v<_ValueType, void>,
            "schema_value_cast only supports SchemaValue, std::string, bool, integral, and double");
    }
}

template <typename _ValueType>
inline auto schema_value_from(const _ValueType& value) -> SchemaValue {
    if constexpr (std::is_same_v<_ValueType, SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, std::string>) {
        return SchemaValue(value);
    } else if constexpr (std::is_same_v<_ValueType, const char*>) {
        return SchemaValue(std::string(value));
    } else if constexpr (std::is_same_v<_ValueType, bool>) {
        return SchemaValue(value);
    } else if constexpr (std::is_floating_point_v<_ValueType>) {
        return SchemaValue(static_cast<double>(value));
    } else if constexpr (std::is_integral_v<_ValueType>) {
        return SchemaValue(static_cast<int64_t>(value));
    } else {
        static_assert(
            std::is_same_v<_ValueType, void>,
            "schema_value_from only supports scalar arithmetic/string types; use SchemaValue::from_vector/from_map for STL");
    }
}

template <typename _ValueType>
inline auto schema_value_from(const std::vector<_ValueType>& value) -> SchemaValue {
    auto out = SchemaArray{};
    out.reserve(value.size());
    for (const auto& item : value) {
        out.emplace_back(schema_value_from(item));
    }
    return SchemaValue(std::move(out));
}

template <typename _ValueType>
inline auto schema_value_from(const std::map<std::string, _ValueType>& value) -> SchemaValue {
    auto out = SchemaObject{};
    for (const auto& [key, item] : value) {
        out.emplace(key, schema_value_from(item));
    }
    return SchemaValue(std::move(out));
}

inline void dump_schema_value(std::ostream& os, const SchemaValue& v, int indent) {
    const auto is_bare_string_safe = [](const std::string& s) -> bool {
        if (s.empty()) {
            return false;
        }
        if (s == "true" || s == "false" || s == "null") {
            return false;
        }
        if (s[0] == '#') {
            return false;
        }
        if (s.size() >= 2 && s[0] == '/' && s[1] == '/') {
            return false;
        }
        for (const auto ch : s) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                return false;
            }
            switch (ch) {
            case '"':
            case '\\':
            case '{':
            case '}':
            case '[':
            case ']':
            case ',':
            case '=':
            case '#':
                return false;
            default:
                break;
            }
        }
        {
            size_t parsed = 0;
            try {
                (void)std::stoll(s, &parsed);
                if (parsed == s.size()) {
                    return false;
                }
            } catch (...) {
            }
        }
        {
            size_t parsed = 0;
            try {
                (void)std::stod(s, &parsed);
                if (parsed == s.size()) {
                    return false;
                }
            } catch (...) {
            }
        }
        return true;
    };

    const auto indent_str = [indent]() -> std::string { return std::string(static_cast<size_t>(indent), ' '); };
    if (v.is_object()) {
        os << "{\n";
        const auto& obj = v.as_object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            os << indent_str() << "    " << it->first << " ";
            if (it->second.is_object() || it->second.is_array()) {
                dump_schema_value(os, it->second, indent + 4);
            } else {
                os << "= ";
                dump_schema_value(os, it->second, indent + 4);
            }
            os << "\n";
        }
        os << indent_str() << "}";
        return;
    }
    if (v.is_array()) {
        os << "[";
        const auto& arr = v.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                os << ", ";
            }
            dump_schema_value(os, arr[i], indent);
        }
        os << "]";
        return;
    }
    const auto& s = v.as_scalar();
    if (std::holds_alternative<std::string>(s)) {
        const auto& str = std::get<std::string>(s);
        if (is_bare_string_safe(str)) {
            os << str;
        } else {
            os << "\"";
            for (const auto ch : str) {
                if (ch == '"' || ch == '\\') {
                    os << "\\";
                }
                os << ch;
            }
            os << "\"";
        }
        return;
    }
    if (std::holds_alternative<int64_t>(s)) {
        os << std::get<int64_t>(s);
        return;
    }
    if (std::holds_alternative<double>(s)) {
        os << std::get<double>(s);
        return;
    }
    if (std::holds_alternative<bool>(s)) {
        os << (std::get<bool>(s) ? "true" : "false");
        return;
    }
    os << "null";
}

inline auto dump_schema_text(const SchemaValue& root) -> std::string {
    if (not root.is_object()) {
        throw star::excep::RuntimeError("dump_schema_text failed, root must be object");
    }
    std::ostringstream oss;
    const auto& obj = root.as_object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        oss << it->first << " ";
        if (it->second.is_object() || it->second.is_array()) {
            dump_schema_value(oss, it->second, 0);
        } else {
            oss << "= ";
            dump_schema_value(oss, it->second, 0);
        }
        oss << "\n\n";
    }
    return oss.str();
}

inline void dump_schema_text_to(const SchemaValue& root, std::ostream& os) {
    if (not root.is_object()) {
        throw star::excep::RuntimeError("dump_schema_text_to failed, root must be object");
    }
    const auto& obj = root.as_object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        os << it->first << " ";
        if (it->second.is_object() || it->second.is_array()) {
            dump_schema_value(os, it->second, 0);
        } else {
            os << "= ";
            dump_schema_value(os, it->second, 0);
        }
        os << "\n\n";
    }
}

inline auto dump_schema_text_into(const SchemaValue& root, char* buffer, size_t capacity) -> size_t {
    if (buffer == nullptr || capacity == 0) {
        throw star::excep::RuntimeError("dump_schema_text_into failed, empty buffer");
    }

    class FixedBufferStreambuf final : public std::streambuf {
    public:
        FixedBufferStreambuf(char* data, size_t cap) : begin_(data), cur_(data), end_(data + cap) {}

        auto size() const -> size_t { return static_cast<size_t>(cur_ - begin_); }

    protected:
        auto overflow(int ch) -> int override {
            if (ch == traits_type::eof()) {
                return traits_type::not_eof(ch);
            }
            if (cur_ >= end_) {
                return traits_type::eof();
            }
            *cur_++ = static_cast<char>(ch);
            return ch;
        }

        auto xsputn(const char* s, std::streamsize n) -> std::streamsize override {
            if (n <= 0) {
                return 0;
            }
            const auto remain = static_cast<std::streamsize>(end_ - cur_);
            const auto to_write = std::min(remain, n);
            if (to_write > 0) {
                std::memcpy(cur_, s, static_cast<size_t>(to_write));
                cur_ += to_write;
            }
            return to_write;
        }

    private:
        char* begin_{nullptr};
        char* cur_{nullptr};
        char* end_{nullptr};
    };

    FixedBufferStreambuf sb(buffer, capacity);
    std::ostream os(&sb);
    dump_schema_text_to(root, os);
    os.flush();
    if (os.fail()) {
        throw star::excep::RuntimeError("dump_schema_text_into failed, buffer too small");
    }
    return sb.size();
}

inline void write_schema_file(const std::string& path, const SchemaValue& root) {
    std::ofstream ofs(path, std::ios::trunc);
    if (not ofs.is_open()) {
        throw star::excep::RuntimeError("write_schema_file failed, cannot open file: ", path);
    }
    ofs << dump_schema_text(root);
}

inline auto find_value(const SchemaObject& obj, const std::string& dotted_key) -> const SchemaValue* {
    const SchemaObject* cur = &obj;
    size_t start = 0;
    while (true) {
        const auto dot = dotted_key.find('.', start);
        const auto part = (dot == std::string::npos) ? dotted_key.substr(start) : dotted_key.substr(start, dot - start);
        if (part.empty()) {
            return nullptr;
        }
        const auto it = cur->find(part);
        if (it == cur->end()) {
            return nullptr;
        }
        if (dot == std::string::npos) {
            return &it->second;
        }
        if (not it->second.is_object()) {
            return nullptr;
        }
        cur = &it->second.as_object();
        start = dot + 1;
    }
}

inline void flatten_object_to_dotted(const SchemaObject& obj,
                                    const std::string& prefix,
                                    std::map<std::string, SchemaValue>& out) {
    for (const auto& [k, v] : obj) {
        const auto name = prefix.empty() ? k : prefix + "." + k;
        if (v.is_object()) {
            flatten_object_to_dotted(v.as_object(), name, out);
        } else {
            out.emplace(name, v);
        }
    }
}

} // namespace star::schema

#endif // __SCHEMA_VALUE_H__
