#ifndef QQ_SCRIPT_MATCHER_H
#define QQ_SCRIPT_MATCHER_H

#include "configuration/configuration.h"
#include "plugin_type.h"

#include <string_view>
#include <vector>

// TODO: change matcher interface to accept Script and check if is match
//       -> current matcher could be "FilesystemGrepMatcher" with only path set
template <typename Matcher>
concept IsScriptMatcher = requires(const Matcher matcher, std::vector<std::filesystem::path> search_paths, std::string_view search_pattern) {
    { matcher.match(search_paths, search_pattern) } -> std::same_as<std::vector<std::filesystem::path>>;
};

class SimpleScriptMatcher {
public:
    SimpleScriptMatcher() = default;

    [[nodiscard]] static std::vector<std::filesystem::path> match(const std::vector<std::filesystem::path>& paths, std::string_view search_pattern);

private:
    [[nodiscard]] static bool isFileMatch(const std::filesystem::path& file_path, std::string_view search_pattern);
};

class FunctionNameScriptMatcher {
public:
    explicit FunctionNameScriptMatcher(Configuration* config)
        : config_ { config }
    {
        assert(config_ != nullptr);
    }

    [[nodiscard]] std::vector<std::filesystem::path> match(const std::vector<std::filesystem::path>& paths, std::string_view search_pattern) const;

private:
    [[nodiscard]] bool isFileMatch(const std::filesystem::path& file_path, std::string_view search_pattern) const;

    [[nodiscard]] static bool matchScore(std::string_view matched_line, std::string_view search_pattern, PluginType type);

private:
    Configuration* config_;
};

#endif // QQ_SCRIPT_MATCHER_H
