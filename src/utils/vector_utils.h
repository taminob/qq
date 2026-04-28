#ifndef QQ_UTILS_VECTOR_UTILS_H
#define QQ_UTILS_VECTOR_UTILS_H

#include <algorithm>
#include <ranges>
#include <string_view>
#include <vector>

#include <boost/container_hash/hash.hpp>

template <typename T>
struct VectorHash {
    using HashType = std::hash<std::string_view>;
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    std::size_t operator()(const std::vector<T>& vector)
    {
        std::size_t result { };
        for (auto&& element : vector) {
            boost::hash_combine(result, element);
        }
        return result;
    }
};

template <typename T, typename U>
std::vector<T>& appendVector(std::vector<T>& vector, U&& to_be_added)
{
    if constexpr (std::is_rvalue_reference_v<U&&>) {
        vector.insert(vector.end(), std::move_iterator { to_be_added.begin() }, std::move_iterator { to_be_added.end() });
    } else {
        vector.insert(vector.end(), to_be_added.begin(), to_be_added.end());
    }
    return vector;
}

template <typename Range, typename T>
bool contains(Range&& range, T&& value)
{
    auto it = std::ranges::find(range, std::forward<T>(value));
    return it != range.end();
}

template <typename Range>
void sortAndRemoveDuplicates(Range& range)
{
    std::ranges::sort(range);
    auto [duplicate_begin, duplicate_end] = std::ranges::unique(range);
    range.erase(duplicate_begin, duplicate_end);
}

template <typename T, typename Range>
std::vector<T> toVector(Range&& range, std::size_t size_limit = std::numeric_limits<std::size_t>::max())
{
    if (size_limit != std::numeric_limits<std::size_t>::max()) {
        return std::forward<Range>(range) | std::views::take(size_limit) | std::ranges::to<std::vector<T>>();
    }
    return std::forward<Range>(range) | std::ranges::to<std::vector<T>>();
}

#endif // QQ_UTILS_VECTOR_UTILS_H
