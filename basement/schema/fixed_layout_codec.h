#ifndef __SCHEMA_FIXED_LAYOUT_CODEC_H__
#define __SCHEMA_FIXED_LAYOUT_CODEC_H__

#include "basement/core/exception.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace star::schema {

#pragma pack(push, 1)
struct FixedLayoutHeader {
    uint16_t magic{0x5354};
    uint8_t version{1};
    uint8_t flags{0};
    uint16_t fixed_section_size{0};
    uint32_t total_size{0};
};
#pragma pack(pop)

static_assert(sizeof(FixedLayoutHeader) == 10, "FixedLayoutHeader must be 10 bytes");

struct FixedFieldSlot {
    union {
        bool bool_val;
        int64_t int64_val;
        double float64_val;
        struct {
            uint32_t offset;
            uint32_t length;
        } var_ref;
    } data{};
    uint8_t type_tag{0};

    static constexpr uint8_t kTagNull = 0;
    static constexpr uint8_t kTagBool = 1;
    static constexpr uint8_t kTagInt64 = 2;
    static constexpr uint8_t kTagFloat64 = 3;
    static constexpr uint8_t kTagString = 4;
    static constexpr uint8_t kTagObject = 5;
    static constexpr uint8_t kTagArray = 6;
    static constexpr uint8_t kTagMap = 7;
};

static_assert(sizeof(FixedFieldSlot) == 16, "FixedFieldSlot must be 16 bytes");

class FixedLayoutWriter {
  public:
    explicit FixedLayoutWriter(size_t field_count) { fixed_slots_.resize(field_count); }

    void set_bool(size_t index, bool value) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagBool;
        slot.data.bool_val = value;
    }

    void set_int64(size_t index, int64_t value) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagInt64;
        slot.data.int64_val = value;
    }

    void set_float64(size_t index, double value) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagFloat64;
        slot.data.float64_val = value;
    }

    void set_string(size_t index, std::string_view value) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagString;
        slot.data.var_ref.offset = static_cast<uint32_t>(var_data_.size());
        slot.data.var_ref.length = static_cast<uint32_t>(value.size());
        var_data_.insert(var_data_.end(), value.begin(), value.end());
    }

    void set_object(size_t index, std::span<const uint8_t> encoded_object) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagObject;
        slot.data.var_ref.offset = static_cast<uint32_t>(var_data_.size());
        slot.data.var_ref.length = static_cast<uint32_t>(encoded_object.size());
        var_data_.insert(var_data_.end(), encoded_object.begin(), encoded_object.end());
    }

    void set_array(size_t index, std::span<const uint8_t> encoded_array) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagArray;
        slot.data.var_ref.offset = static_cast<uint32_t>(var_data_.size());
        slot.data.var_ref.length = static_cast<uint32_t>(encoded_array.size());
        var_data_.insert(var_data_.end(), encoded_array.begin(), encoded_array.end());
    }

    void set_map(size_t index, std::span<const uint8_t> encoded_map) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagMap;
        slot.data.var_ref.offset = static_cast<uint32_t>(var_data_.size());
        slot.data.var_ref.length = static_cast<uint32_t>(encoded_map.size());
        var_data_.insert(var_data_.end(), encoded_map.begin(), encoded_map.end());
    }

    void set_null(size_t index) {
        if (index >= fixed_slots_.size()) { throw star::excep::RuntimeError("FixedLayoutWriter: index out of bounds"); }
        auto& slot = fixed_slots_[index];
        slot.type_tag = FixedFieldSlot::kTagNull;
    }

    auto serialize() const -> std::vector<uint8_t> {
        const auto fixed_size = sizeof(FixedLayoutHeader) + fixed_slots_.size() * sizeof(FixedFieldSlot);
        const auto total_size = fixed_size + var_data_.size();

        auto out = std::vector<uint8_t>(total_size);
        auto* header = reinterpret_cast<FixedLayoutHeader*>(out.data());
        header->magic = 0x5354;
        header->version = 1;
        header->flags = 0;
        header->fixed_section_size = static_cast<uint16_t>(fixed_size);
        header->total_size = static_cast<uint32_t>(total_size);

        if (not fixed_slots_.empty()) {
            std::memcpy(out.data() + sizeof(FixedLayoutHeader), fixed_slots_.data(), fixed_slots_.size() * sizeof(FixedFieldSlot));
        }
        if (not var_data_.empty()) {
            std::memcpy(out.data() + fixed_size, var_data_.data(), var_data_.size());
        }
        return out;
    }

    auto serialize_into(uint8_t* buf, size_t cap) const -> size_t {
        const auto fixed_size = sizeof(FixedLayoutHeader) + fixed_slots_.size() * sizeof(FixedFieldSlot);
        const auto total_size = fixed_size + var_data_.size();
        if (total_size > cap) {
            return 0;
        }
        auto* header = reinterpret_cast<FixedLayoutHeader*>(buf);
        header->magic = 0x5354;
        header->version = 1;
        header->flags = 0;
        header->fixed_section_size = static_cast<uint16_t>(fixed_size);
        header->total_size = static_cast<uint32_t>(total_size);

        if (not fixed_slots_.empty()) {
            std::memcpy(buf + sizeof(FixedLayoutHeader), fixed_slots_.data(), fixed_slots_.size() * sizeof(FixedFieldSlot));
        }
        if (not var_data_.empty()) {
            std::memcpy(buf + fixed_size, var_data_.data(), var_data_.size());
        }
        return total_size;
    }

  private:
    std::vector<FixedFieldSlot> fixed_slots_;
    std::vector<uint8_t> var_data_;
};

class FixedLayoutView {
  public:
    FixedLayoutView() = default;
    explicit FixedLayoutView(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    auto valid() const -> bool {
        if (data_ == nullptr || size_ < sizeof(FixedLayoutHeader)) {
            return false;
        }
        const auto* header = reinterpret_cast<const FixedLayoutHeader*>(data_);
        return header->magic == 0x5354 && header->version == 1;
    }

    auto field_count() const -> size_t {
        if (!valid()) return 0;
        const auto* header = reinterpret_cast<const FixedLayoutHeader*>(data_);
        const auto fixed_payload = header->fixed_section_size - sizeof(FixedLayoutHeader);
        return fixed_payload / sizeof(FixedFieldSlot);
    }

    auto get_bool(size_t index) const -> std::optional<bool> {
        auto* slot = get_slot(index);
        if (slot == nullptr || slot->type_tag != FixedFieldSlot::kTagBool) return std::nullopt;
        return slot->data.bool_val;
    }

    auto get_int64(size_t index) const -> std::optional<int64_t> {
        auto* slot = get_slot(index);
        if (slot == nullptr || slot->type_tag != FixedFieldSlot::kTagInt64) return std::nullopt;
        return slot->data.int64_val;
    }

    auto get_float64(size_t index) const -> std::optional<double> {
        auto* slot = get_slot(index);
        if (slot == nullptr || slot->type_tag != FixedFieldSlot::kTagFloat64) return std::nullopt;
        return slot->data.float64_val;
    }

    auto get_string(size_t index) const -> std::optional<std::string_view> {
        auto* slot = get_slot(index);
        if (slot == nullptr || slot->type_tag != FixedFieldSlot::kTagString) return std::nullopt;
        const auto fixed_size = reinterpret_cast<const FixedLayoutHeader*>(data_)->fixed_section_size;
        const auto var_base = data_ + fixed_size;
        if (slot->data.var_ref.offset + slot->data.var_ref.length > size_ - fixed_size) {
            return std::nullopt;
        }
        return std::string_view(reinterpret_cast<const char*>(var_base + slot->data.var_ref.offset), slot->data.var_ref.length);
    }

    auto get_object_bytes(size_t index) const -> std::optional<std::span<const uint8_t>> { return get_var_ref_bytes(index, FixedFieldSlot::kTagObject); }

    auto get_array_bytes(size_t index) const -> std::optional<std::span<const uint8_t>> { return get_var_ref_bytes(index, FixedFieldSlot::kTagArray); }

    auto get_map_bytes(size_t index) const -> std::optional<std::span<const uint8_t>> { return get_var_ref_bytes(index, FixedFieldSlot::kTagMap); }

    auto is_null(size_t index) const -> bool {
        auto* slot = get_slot(index);
        return slot == nullptr || slot->type_tag == FixedFieldSlot::kTagNull;
    }

    auto type_tag(size_t index) const -> uint8_t {
        auto* slot = get_slot(index);
        return slot ? slot->type_tag : FixedFieldSlot::kTagNull;
    }

    auto raw_data() const -> const uint8_t* { return data_; }
    auto raw_size() const -> size_t { return size_; }

  private:
    auto get_slot(size_t index) const -> const FixedFieldSlot* {
        if (!valid()) return nullptr;
        if (index >= field_count()) return nullptr;
        return reinterpret_cast<const FixedFieldSlot*>(data_ + sizeof(FixedLayoutHeader)) + index;
    }

    auto get_var_ref_bytes(size_t index, uint8_t expected_tag) const -> std::optional<std::span<const uint8_t>> {
        auto* slot = get_slot(index);
        if (slot == nullptr || slot->type_tag != expected_tag) return std::nullopt;
        const auto fixed_size = reinterpret_cast<const FixedLayoutHeader*>(data_)->fixed_section_size;
        const auto var_base = data_ + fixed_size;
        if (slot->data.var_ref.offset + slot->data.var_ref.length > size_ - fixed_size) {
            return std::nullopt;
        }
        return std::span<const uint8_t>(var_base + slot->data.var_ref.offset, slot->data.var_ref.length);
    }

    const uint8_t* data_{nullptr};
    size_t size_{0};
};

template <typename _T>
auto encode_primitive_array(std::span<const _T> values) -> std::vector<uint8_t> {
    auto out = std::vector<uint8_t>(4 + values.size() * sizeof(_T));
    auto count = static_cast<uint32_t>(values.size());
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    if (not values.empty()) {
        std::memcpy(out.data() + 4, values.data(), values.size() * sizeof(_T));
    }
    return out;
}

template <typename _T>
auto decode_primitive_array(std::span<const uint8_t> data) -> std::vector<_T> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    if (data.size() < 4 + static_cast<size_t>(count) * sizeof(_T)) {
        return {};
    }
    auto out = std::vector<_T>(count);
    if (count > 0) {
        std::memcpy(out.data(), data.data() + 4, static_cast<size_t>(count) * sizeof(_T));
    }
    return out;
}

inline auto encode_bool_array(const std::vector<bool>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    auto out = std::vector<uint8_t>(4 + values.size());
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    for (size_t i = 0; i < values.size(); ++i) {
        out[4 + i] = values[i] ? 1 : 0;
    }
    return out;
}

inline auto decode_bool_array(std::span<const uint8_t> data) -> std::vector<bool> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    if (data.size() < 4 + count) {
        return {};
    }
    auto out = std::vector<bool>(count);
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = data[4 + i] != 0;
    }
    return out;
}

inline auto encode_string_array(const std::vector<std::string>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    size_t total = 4;
    for (const auto& s : values) {
        total += 4 + s.size();
    }
    auto out = std::vector<uint8_t>(total, 0);
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    size_t pos = 4;
    for (const auto& s : values) {
        auto len = static_cast<uint32_t>(s.size());
        std::memcpy(out.data() + pos, &len, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, s.data(), s.size());
        pos += s.size();
    }
    return out;
}

inline auto decode_string_array(std::span<const uint8_t> data) -> std::vector<std::string> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    auto out = std::vector<std::string>{};
    out.reserve(count);
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > data.size()) {
            return {};
        }
        auto len = uint32_t{0};
        std::memcpy(&len, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + len > data.size()) {
            return {};
        }
        out.emplace_back(reinterpret_cast<const char*>(data.data() + pos), len);
        pos += len;
    }
    return out;
}

template <typename _T>
auto encode_primitive_map(const std::map<std::string, _T>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    size_t total = 4;
    for (const auto& [k, v] : values) {
        total += 4 + k.size() + sizeof(_T);
    }
    auto out = std::vector<uint8_t>(total, 0);
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    size_t pos = 4;
    for (const auto& [k, v] : values) {
        auto klen = static_cast<uint32_t>(k.size());
        std::memcpy(out.data() + pos, &klen, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, k.data(), k.size());
        pos += k.size();
        std::memcpy(out.data() + pos, &v, sizeof(_T));
        pos += sizeof(_T);
    }
    return out;
}

template <typename _T>
auto decode_primitive_map(std::span<const uint8_t> data) -> std::map<std::string, _T> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    auto out = std::map<std::string, _T>{};
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > data.size()) {
            return {};
        }
        auto klen = uint32_t{0};
        std::memcpy(&klen, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + klen + sizeof(_T) > data.size()) {
            return {};
        }
        auto key = std::string(reinterpret_cast<const char*>(data.data() + pos), klen);
        pos += klen;
        auto val = _T{};
        std::memcpy(&val, data.data() + pos, sizeof(_T));
        pos += sizeof(_T);
        out.emplace(std::move(key), val);
    }
    return out;
}

inline auto encode_string_map(const std::map<std::string, std::string>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    size_t total = 4;
    for (const auto& [k, v] : values) {
        total += 4 + k.size() + 4 + v.size();
    }
    auto out = std::vector<uint8_t>(total, 0);
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    size_t pos = 4;
    for (const auto& [k, v] : values) {
        auto klen = static_cast<uint32_t>(k.size());
        std::memcpy(out.data() + pos, &klen, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, k.data(), k.size());
        pos += k.size();
        auto vlen = static_cast<uint32_t>(v.size());
        std::memcpy(out.data() + pos, &vlen, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, v.data(), v.size());
        pos += v.size();
    }
    return out;
}

inline auto decode_string_map(std::span<const uint8_t> data) -> std::map<std::string, std::string> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    auto out = std::map<std::string, std::string>{};
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > data.size()) {
            return {};
        }
        auto klen = uint32_t{0};
        std::memcpy(&klen, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + klen > data.size()) {
            return {};
        }
        auto key = std::string(reinterpret_cast<const char*>(data.data() + pos), klen);
        pos += klen;
        if (pos + 4 > data.size()) {
            return {};
        }
        auto vlen = uint32_t{0};
        std::memcpy(&vlen, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + vlen > data.size()) {
            return {};
        }
        auto val = std::string(reinterpret_cast<const char*>(data.data() + pos), vlen);
        pos += vlen;
        out.emplace(std::move(key), std::move(val));
    }
    return out;
}

template <typename _T>
auto encode_object_array(const std::vector<_T>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    size_t total = 4;
    std::vector<std::vector<uint8_t>> encoded;
    encoded.reserve(values.size());
    for (const auto& v : values) {
        encoded.push_back(v.encode_fixed_layout());
        total += 4 + encoded.back().size();
    }
    auto out = std::vector<uint8_t>(total, 0);
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    size_t pos = 4;
    for (const auto& e : encoded) {
        auto len = static_cast<uint32_t>(e.size());
        std::memcpy(out.data() + pos, &len, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, e.data(), e.size());
        pos += e.size();
    }
    return out;
}

template <typename _T>
auto decode_object_array(std::span<const uint8_t> data) -> std::vector<_T> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    auto out = std::vector<_T>{};
    out.reserve(count);
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > data.size()) {
            return {};
        }
        auto len = uint32_t{0};
        std::memcpy(&len, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + len > data.size()) {
            return {};
        }
        auto elem = _T::from_fixed_layout(std::span<const uint8_t>(data.data() + pos, len));
        out.push_back(std::move(elem));
        pos += len;
    }
    return out;
}

template <typename _T>
auto encode_object_map(const std::map<std::string, _T>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    size_t total = 4;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> encoded;
    encoded.reserve(values.size());
    for (const auto& [k, v] : values) {
        encoded.emplace_back(k, v.encode_fixed_layout());
        total += 4 + k.size() + 4 + encoded.back().second.size();
    }
    auto out = std::vector<uint8_t>(total, 0);
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    size_t pos = 4;
    for (const auto& [k, v_bytes] : encoded) {
        auto klen = static_cast<uint32_t>(k.size());
        std::memcpy(out.data() + pos, &klen, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, k.data(), k.size());
        pos += k.size();
        auto vlen = static_cast<uint32_t>(v_bytes.size());
        std::memcpy(out.data() + pos, &vlen, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, v_bytes.data(), v_bytes.size());
        pos += v_bytes.size();
    }
    return out;
}

template <typename _T>
auto decode_object_map(std::span<const uint8_t> data) -> std::map<std::string, _T> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    auto out = std::map<std::string, _T>{};
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > data.size()) {
            return {};
        }
        auto klen = uint32_t{0};
        std::memcpy(&klen, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + klen > data.size()) {
            return {};
        }
        auto key = std::string(reinterpret_cast<const char*>(data.data() + pos), klen);
        pos += klen;
        if (pos + 4 > data.size()) {
            return {};
        }
        auto vlen = uint32_t{0};
        std::memcpy(&vlen, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + vlen > data.size()) {
            return {};
        }
        auto val = _T::from_fixed_layout(std::span<const uint8_t>(data.data() + pos, vlen));
        pos += vlen;
        out.emplace(std::move(key), std::move(val));
    }
    return out;
}

template <typename _T>
auto encode_binary_value_array(const std::vector<_T>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    size_t total = 4;
    std::vector<std::vector<uint8_t>> encoded;
    encoded.reserve(values.size());
    for (const auto& v : values) {
        encoded.push_back(v.encode_binary());
        total += 4 + encoded.back().size();
    }
    auto out = std::vector<uint8_t>(total, 0);
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    size_t pos = 4;
    for (const auto& e : encoded) {
        auto len = static_cast<uint32_t>(e.size());
        std::memcpy(out.data() + pos, &len, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, e.data(), e.size());
        pos += e.size();
    }
    return out;
}

template <typename _T>
auto decode_binary_value_array(std::span<const uint8_t> data) -> std::vector<_T> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    auto out = std::vector<_T>{};
    out.reserve(count);
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > data.size()) {
            return {};
        }
        auto len = uint32_t{0};
        std::memcpy(&len, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + len > data.size()) {
            return {};
        }
        auto elem = _T::from_binary(std::span<const uint8_t>(data.data() + pos, len));
        out.push_back(std::move(elem));
        pos += len;
    }
    return out;
}

template <typename _T>
auto encode_binary_value_map(const std::map<std::string, _T>& values) -> std::vector<uint8_t> {
    auto count = static_cast<uint32_t>(values.size());
    size_t total = 4;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> encoded;
    encoded.reserve(values.size());
    for (const auto& [k, v] : values) {
        encoded.emplace_back(k, v.encode_binary());
        total += 4 + k.size() + 4 + encoded.back().second.size();
    }
    auto out = std::vector<uint8_t>(total, 0);
    std::memcpy(out.data(), &count, sizeof(uint32_t));
    size_t pos = 4;
    for (const auto& [k, v_bytes] : encoded) {
        auto klen = static_cast<uint32_t>(k.size());
        std::memcpy(out.data() + pos, &klen, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, k.data(), k.size());
        pos += k.size();
        auto vlen = static_cast<uint32_t>(v_bytes.size());
        std::memcpy(out.data() + pos, &vlen, sizeof(uint32_t));
        pos += 4;
        std::memcpy(out.data() + pos, v_bytes.data(), v_bytes.size());
        pos += v_bytes.size();
    }
    return out;
}

template <typename _T>
auto decode_binary_value_map(std::span<const uint8_t> data) -> std::map<std::string, _T> {
    if (data.size() < 4) {
        return {};
    }
    auto count = uint32_t{0};
    std::memcpy(&count, data.data(), sizeof(uint32_t));
    auto out = std::map<std::string, _T>{};
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > data.size()) {
            return {};
        }
        auto klen = uint32_t{0};
        std::memcpy(&klen, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + klen > data.size()) {
            return {};
        }
        auto key = std::string(reinterpret_cast<const char*>(data.data() + pos), klen);
        pos += klen;
        if (pos + 4 > data.size()) {
            return {};
        }
        auto vlen = uint32_t{0};
        std::memcpy(&vlen, data.data() + pos, sizeof(uint32_t));
        pos += 4;
        if (pos + vlen > data.size()) {
            return {};
        }
        auto val = _T::from_binary(std::span<const uint8_t>(data.data() + pos, vlen));
        pos += vlen;
        out.emplace(std::move(key), std::move(val));
    }
    return out;
}

}  // namespace star::schema

#endif  // __SCHEMA_FIXED_LAYOUT_CODEC_H__
