#ifndef __SCHEMA_DECODE_POLICY_H__
#define __SCHEMA_DECODE_POLICY_H__

#include <cstdint>
#include <string_view>

namespace star::schema {

// 版本不一致时的解码策略（按通道配置，见 docs/contract_decisions.md 决策二/三）。
//
// 三个维度分别对应三类不一致：
//   unknown       生产者有、消费者 schema 里没有的字段
//   missing       消费者 schema 里有、生产者没发的字段
//   type_mismatch 同一字段 id 的类型不同
//
// 编译期字段删除/改名不在此列——源码引用了不存在的字段，编译就该失败。

enum class UnknownFieldPolicy : uint8_t {
    kError = 0,  // 出现不认识的字段就报错
    kKeep = 1,   // 保留原始 TLV，随消息继续传递（默认）
    kDrop = 2,   // 丢弃未知字段
};

enum class MissingFieldPolicy : uint8_t {
    kError = 0,    // 缺字段就报错（严格）
    kDefault = 1,  // 用 schema 默认值补；无默认值且非 optional 时仍报错（默认）
};

enum class TypeMismatchPolicy : uint8_t {
    kError = 0,       // 类型不同就报错（默认）
    kBestEffort = 1,  // 仅在可安全转换时转换，否则报错
};

struct DecodePolicy {
    UnknownFieldPolicy unknown{UnknownFieldPolicy::kKeep};
    MissingFieldPolicy missing{MissingFieldPolicy::kDefault};
    TypeMismatchPolicy type_mismatch{TypeMismatchPolicy::kError};

    // 严格模式：任何版本差异都报错。适用于「必须同版本」的通道（固定布局、控制指令）。
    static auto strict() -> DecodePolicy {
        return DecodePolicy{UnknownFieldPolicy::kError, MissingFieldPolicy::kError, TypeMismatchPolicy::kError};
    }
};

inline auto operator==(const DecodePolicy& lhs, const DecodePolicy& rhs) -> bool {
    return lhs.unknown == rhs.unknown and lhs.missing == rhs.missing and lhs.type_mismatch == rhs.type_mismatch;
}

inline auto parse_unknown_field_policy(std::string_view text, UnknownFieldPolicy* out) -> bool {
    if (text.empty()) {
        return true;
    }
    if (text == "error") {
        *out = UnknownFieldPolicy::kError;
        return true;
    }
    if (text == "keep") {
        *out = UnknownFieldPolicy::kKeep;
        return true;
    }
    if (text == "drop") {
        *out = UnknownFieldPolicy::kDrop;
        return true;
    }
    return false;
}

inline auto parse_missing_field_policy(std::string_view text, MissingFieldPolicy* out) -> bool {
    if (text.empty()) {
        return true;
    }
    if (text == "error") {
        *out = MissingFieldPolicy::kError;
        return true;
    }
    if (text == "default") {
        *out = MissingFieldPolicy::kDefault;
        return true;
    }
    return false;
}

inline auto parse_type_mismatch_policy(std::string_view text, TypeMismatchPolicy* out) -> bool {
    if (text.empty()) {
        return true;
    }
    if (text == "error") {
        *out = TypeMismatchPolicy::kError;
        return true;
    }
    if (text == "best_effort") {
        *out = TypeMismatchPolicy::kBestEffort;
        return true;
    }
    return false;
}

// conf 三种取值解析成一个策略；空字符串表示用默认值。
// 返回 false 表示取值非法，调用方负责带上通道名报错。
inline auto parse_decode_policy(std::string_view unknown, std::string_view missing, std::string_view mismatch, DecodePolicy* out) -> bool {
    if (out == nullptr) {
        return false;
    }
    DecodePolicy result{};
    if (not parse_unknown_field_policy(unknown, &result.unknown) or
        not parse_missing_field_policy(missing, &result.missing) or
        not parse_type_mismatch_policy(mismatch, &result.type_mismatch)) {
        return false;
    }
    *out = result;
    return true;
}

}  // namespace star::schema

#endif  // __SCHEMA_DECODE_POLICY_H__
