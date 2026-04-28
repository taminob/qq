#include "plugin_type.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace {
constexpr auto PLUGIN_TYPE_STRINGS = std::array {
    std::string_view { "unknown" },
    std::string_view { "shell" },
    std::string_view { "python" },
    std::string_view { "lua" },
    std::string_view { "c" },
    std::string_view { "cpp" },
};
} // namespace

template <>
std::optional<PluginType> fromString(std::string_view string)
{
    // NOLINTNEXTLINE(readability-qualified-auto); bug in clang-tidy for std::array::iterator
    auto it = std::ranges::find(PLUGIN_TYPE_STRINGS, string);
    if (it == PLUGIN_TYPE_STRINGS.end()) {
        return std::nullopt;
    }
    auto type = std::distance(PLUGIN_TYPE_STRINGS.begin(), it);
    return static_cast<PluginType>(type);
}

std::string_view toString(PluginType type)
{
    return PLUGIN_TYPE_STRINGS.at(std::to_underlying(type));
}

PluginType detectPluginType(const Configuration& config, const std::filesystem::path& plugin_path)
{
    auto extension = plugin_path.extension().string();
    if (contains(config.getShellExtensions(), extension)) {
        return PluginType::shell;
    }
    if (contains(config.getPythonExtensions(), extension)) {
        return PluginType::python;
    }
    if (contains(config.getLuaExtensions(), extension)) {
        return PluginType::lua;
    }
    if (contains(config.getCExtensions(), extension)) {
        return PluginType::c;
    }
    if (contains(config.getCppExtensions(), extension)) {
        return PluginType::cpp;
    }
    return PluginType::unknown;
}

std::istream& operator>>(std::istream& in, PluginType& type)
{
    std::string token;
    in >> token;
    if (auto parsed = fromString<PluginType>(token)) {
        type = parsed.value_or(PluginType::unknown);
    } else {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& operator<<(std::ostream& out, PluginType type)
{
    return out << toString(type);
}

std::optional<std::string> embedInFunction(PluginType type, const std::string& function_name, const std::string& function_body)
{
    auto determine_indent = [](std::string_view body) -> std::int64_t {
        auto count_leading_whitespaces = [](auto&& line) {
            return std::ranges::distance(line | std::views::take_while([](auto&& character) {
                return character == ' ';
            }));
        };
        constexpr auto DEFAULT_INDENT = 2;

        auto all_indented_lines = body | std::views::split('\n') | std::views::filter([](auto&& line) {
            return !line.empty() && std::isspace(line.front());
        });
        if (!all_indented_lines.empty() && std::ranges::all_of(all_indented_lines, [](auto&& line) { return line.front() == '\t'; })) {
            // tab indentation detected
            return -1;
        }

        auto all_space_indentations = all_indented_lines | std::views::transform(count_leading_whitespaces);
        auto indent = std::ranges::fold_left(all_space_indentations, 0, [](auto&& lhs, auto&& rhs) { return std::gcd(lhs, rhs); });
        if (indent == 0) {
            // no indent detected, use default
            return DEFAULT_INDENT;
        }
        return indent;
    };
    auto indent = [](std::string_view body, std::int64_t indent) {
        std::string result;
        for (auto&& line : std::views::split(body, '\n')) {
            if (indent > 0) {
                result += std::string(indent, ' ');
            } else {
                result += '\t';
            }
            result += std::string_view { line.begin(), line.end() };
            result += '\n';
        }
        return result;
    };

    auto indentation = determine_indent(function_body);
    auto indented_body = indent(function_body, indentation);
    // TODO? add noop if function body may not be empty? (in Shell ":", in Python "pass")
    switch (type) {
    case PluginType::shell:
        return function_name + "() {\n" + indented_body + "}\n";
    case PluginType::python:
        return "def " + function_name + "():\n" + indented_body;
    case PluginType::lua:
        return "function " + function_name + " ()\n" + indented_body + "end\n";
    case PluginType::c:
        [[fallthrough]];
    case PluginType::cpp:
        [[fallthrough]];
    case PluginType::unknown:
        return std::nullopt;
    }
}
