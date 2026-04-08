#ifndef QQ_UTILS_H
#define QQ_UTILS_H

#include <string>
#include <string_view>
#include <vector>

struct StringHash {
    using HashType = std::hash<std::string_view>;
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    std::size_t operator()(const char* str) const { return HashType { }(str); }
    std::size_t operator()(std::string_view str) const { return HashType { }(str); }
    std::size_t operator()(const std::string& str) const { return HashType { }(str); }
};

template <typename T, typename U>
void appendVector(std::vector<T>& vector, U&& to_be_added)
{
    if constexpr (std::is_rvalue_reference_v<U&&>) {
        vector.insert(vector.end(), std::move_iterator { to_be_added.begin() }, std::move_iterator { to_be_added.end() });
    } else {
        vector.insert(vector.end(), to_be_added.begin(), to_be_added.end());
    }
}

template <typename Range, typename T>
bool contains(Range&& range, T&& value)
{
    auto it = std::ranges::find(range, std::forward<T>(value));
    return it != range.end();
}

#endif // QQ_UTILS_H
