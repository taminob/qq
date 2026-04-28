#ifndef QQ_UTILS_STRING_UTILS_H
#define QQ_UTILS_STRING_UTILS_H

#include <string>
#include <string_view>

#include <boost/container_hash/hash.hpp>

struct StringHash {
    using HashType = std::hash<std::string_view>;
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    std::size_t operator()(const char* str) const { return HashType { }(str); }
    std::size_t operator()(std::string_view str) const { return HashType { }(str); }
    std::size_t operator()(const std::string& str) const { return HashType { }(str); }
};

#endif // QQ_UTILS_STRING_UTILS_H
