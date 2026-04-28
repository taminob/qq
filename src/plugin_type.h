#ifndef QQ_PLUGIN_TYPE_H
#define QQ_PLUGIN_TYPE_H

#include "configuration/configuration.h"
#include <cstdint>
#include <istream>
#include <ostream>
#include <string_view>

enum class PluginType : std::uint8_t {
    unknown,
    shell,
    python,
    lua,
    c,
    cpp,
};

template <typename T>
[[nodiscard]] std::optional<T> fromString(std::string_view string);

[[nodiscard]] std::string_view toString(PluginType type);

[[nodiscard]] PluginType detectPluginType(const Configuration& config, const std::filesystem::path& plugin_path);

std::istream& operator>>(std::istream& in, PluginType& type);
std::ostream& operator<<(std::ostream& out, PluginType type);

/**
 * Return full function definition for given function components.
 */
std::optional<std::string> embedInFunction(PluginType type, const std::string& function_name, const std::string& function_body); // TODO: add support for parameters

#endif // QQ_PLUGIN_TYPE_H
