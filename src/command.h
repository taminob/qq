#ifndef QQ_COMMAND_H
#define QQ_COMMAND_H

#include "configuration.h"

#include <filesystem>
#include <string>
#include <utility>

class Command {
public:
    enum class PluginType : std::uint8_t {
        unknown,
        shell,
        python,
        lua,
        c,
        cpp,
    };

public:
    Command(std::filesystem::path plugin_path, PluginType plugin_type, std::string function_name)
        : plugin_path_ { std::move(plugin_path) }
        , plugin_type_ { plugin_type }
        , function_name_ { std::move(function_name) }
    {
    }

    [[nodiscard]] static PluginType detectPluginType(const Configuration& config, const std::filesystem::path& plugin_path)
    {
        auto extension = plugin_path.extension();
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

private:
private:
    std::filesystem::path plugin_path_;
    PluginType plugin_type_;
    std::string function_name_;
};

#endif // QQ_COMMAND_H
