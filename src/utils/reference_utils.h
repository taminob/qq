#ifndef QQ_UTILS_REFERENCE_UTILS_H
#define QQ_UTILS_REFERENCE_UTILS_H

#include <functional>

template <typename T>
struct ReferenceWrapperLess {
    [[nodiscard]] bool operator()(const std::reference_wrapper<T>& lhs,
        const std::reference_wrapper<T>& rhs) const
    {
        return std::addressof(lhs.get()) < std::addressof(rhs.get());
    }
};

template <typename T>
struct ReferenceWrapperHash {
    [[nodiscard]] std::size_t operator()(const std::reference_wrapper<T>& value) const
    {
        return std::hash<T*> { }(std::addressof(value.get()));
    }
};

template <typename T>
struct ReferenceWrapperEqualTo {
    [[nodiscard]] bool operator()(const std::reference_wrapper<T>& lhs,
        const std::reference_wrapper<T>& rhs) const
    {
        return std::addressof(lhs.get()) == std::addressof(rhs.get());
    }
};

#endif // QQ_UTILS_REFERENCE_UTILS_H
