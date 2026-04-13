#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__

#include <memory>
#include <typeinfo>

namespace star::base {

struct TypedConstPtr {
    std::shared_ptr<const void> ptr;
    const std::type_info* type{nullptr};

    template <typename _T>
    static auto from(std::shared_ptr<const _T> p) -> TypedConstPtr {
        TypedConstPtr out;
        out.ptr = std::static_pointer_cast<const void>(std::move(p));
        out.type = &typeid(_T);
        return out;
    }

    template <typename _T>
    static auto from(std::shared_ptr<_T> p) -> TypedConstPtr {
        TypedConstPtr out;
        out.ptr = std::static_pointer_cast<const void>(std::move(p));
        out.type = &typeid(_T);
        return out;
    }

    template <typename _T>
    auto as() const -> std::shared_ptr<const _T> {
        if (not ptr) {
            return nullptr;
        }
        if (type  and  *type != typeid(_T)) {
            return nullptr;
        }
        return std::static_pointer_cast<const _T>(ptr);
    }
};

struct TypedPtr {
    std::shared_ptr<void> ptr;
    const std::type_info* type{nullptr};

    template <typename _T>
    static auto from(std::shared_ptr<_T> p) -> TypedPtr {
        TypedPtr out;
        out.ptr = std::static_pointer_cast<void>(std::move(p));
        out.type = &typeid(_T);
        return out;
    }

    template <typename _T>
    auto as() const -> std::shared_ptr<_T> {
        if (not ptr) {
            return nullptr;
        }
        if (type  and  *type != typeid(_T)) {
            return nullptr;
        }
        return std::static_pointer_cast<_T>(ptr);
    }
};

} // namespace star::base

#endif // __COMMON_TYPES_H__
