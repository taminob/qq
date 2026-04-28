#ifndef QQ_SCRIPT_H
#define QQ_SCRIPT_H

#include "plugin_type.h"
#include "utils/float_utils.h"

#include <filesystem>
#include <string>
#include <utility>

class Script {
public:
    Script(const std::filesystem::path& plugin_path, PluginType plugin_type, std::string function_name, std::vector<std::string> tags = { }, double score = 0.0)
        : plugin_path_ { std::filesystem::weakly_canonical(plugin_path) }
        , plugin_type_ { plugin_type }
        , function_name_ { std::move(function_name) }
        , tags_ { std::move(tags) }
        , score_ { score }
    {
        std::ranges::sort(tags_);
    }

    [[nodiscard]] auto operator<=>(const Script&) const = default;

    void updatePath(const std::filesystem::path& path)
    {
        plugin_path_ = path;
    }

    void updateScore(double score)
    {
        score_ = score;
    }

    /**
     * Return path to script in canonical representation.
     */
    [[nodiscard]] const std::filesystem::path& pluginPath() const { return plugin_path_; }
    [[nodiscard]] PluginType pluginType() const { return plugin_type_; }
    [[nodiscard]] const std::string& functionName() const { return function_name_; }
    /**
     * Return tags in sorted order.
     */
    [[nodiscard]] const std::vector<std::string>& tags() const { return tags_; }
    [[nodiscard]] double score() const { return score_; }

private:
    std::filesystem::path plugin_path_;
    PluginType plugin_type_;
    std::string function_name_;
    std::vector<std::string> tags_;
    EpsilonEqualityFloatingPoint<double> score_;
};

#endif // QQ_SCRIPT_H
