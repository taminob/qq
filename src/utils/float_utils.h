#ifndef QQ_UTILS_FLOAT_UTILS_H
#define QQ_UTILS_FLOAT_UTILS_H

#include <cmath>
#include <concepts>
#include <limits>
#include <utility>

template <typename T>
class EpsilonEqualityFloatingPoint {
public:
    EpsilonEqualityFloatingPoint() = default;

    template <std::convertible_to<T> U>
    EpsilonEqualityFloatingPoint(U&& value) // NOLINT(google-explicit-constructor, hicpp-explicit-conversions)
        : value_ { std::forward<U>(value) }
    {
    }

    operator T() const // NOLINT(google-explicit-constructor, hicpp-explicit-conversions)
    {
        return value_;
    }

    /**
     * Return equal if the other value is the one of the adjacent representable values,
     * otherwise use default three-way comparison.
     */
    template <typename U>
    std::partial_ordering operator<=>(const EpsilonEqualityFloatingPoint<U>& rhs) const
    {
        // calculate next smaller value representable by the floating point type; this is used
        // to check if the RHS is this almost same value
        auto&& next_smaller_value = std::nextafter(value_, std::numeric_limits<T>::lowest());
        // calculate next larger value representable by the floating point type to also check
        // for almost equal in the other direction
        auto&& next_larger_value = std::nextafter(value_, std::numeric_limits<T>::max());
        if (next_smaller_value <= rhs.value_ && next_larger_value >= rhs.value_) {
            return std::partial_ordering::equivalent;
        }
        return value_ <=> rhs.value_;
    }

private:
    T value_ { };
};
using EpsilonEqualityDouble = EpsilonEqualityFloatingPoint<double>;

#endif // QQ_UTILS_FLOAT_UTILS_H
