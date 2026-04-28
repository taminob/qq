#ifndef QQ_CONFIGURATION_EXTENSIONS_H
#define QQ_CONFIGURATION_EXTENSIONS_H

#include "configuration_option.h"
#include "ui.h"
#include "utils/vector_utils.h"

#include <string>
#include <vector>

#include <ppplugin/plugin.h>

class ExtensionsConfiguration {
public:
    ExtensionsConfiguration() = default;

    void read(ppplugin::Plugin& plugin)
    {
        auto read_extensions = [&](auto& extensions, const std::string& variable_name) {
            auto new_value = plugin.global<std::vector<std::string>>(variable_name).valueOr(std::vector<std::string> { });
            extensions.add(ConfigurationLayer::file, new_value, [](auto prev, auto&& new_value) {
                appendVector(prev, new_value);
                return prev;
            });
        };
        read_extensions(shell_extensions_, "SHELL_EXTENSIONS");
        read_extensions(python_extensions_, "PYTHON_EXTENSIONS");
        read_extensions(lua_extensions_, "LUA_EXTENSIONS");
        read_extensions(c_extensions_, "C_EXTENSIONS");
        read_extensions(cpp_extensions_, "CPP_EXTENSIONS");
    }

    void normalize()
    {
        shell_extensions_.removeDuplicatesAcrossLayers();
        python_extensions_.removeDuplicatesAcrossLayers();
        lua_extensions_.removeDuplicatesAcrossLayers();
        c_extensions_.removeDuplicatesAcrossLayers();
        cpp_extensions_.removeDuplicatesAcrossLayers();
    }

    [[nodiscard]] bool verify(Ui& ui) const
    {
        // an extension may only appear once for all plugin types
        std::vector<std::string> all_extensions;
        appendVector(all_extensions, shellExtensions());
        appendVector(all_extensions, pythonExtensions());
        appendVector(all_extensions, luaExtensions());
        appendVector(all_extensions, cExtensions());
        appendVector(all_extensions, cppExtensions());
        auto [duplicate_begin, duplicate_end] = std::ranges::unique(all_extensions);
        if (duplicate_begin != duplicate_end) {
            auto&& error = ui.error();
            error << "Found duplicates in extensions for plugin type detection: " << *duplicate_begin++;
            while (duplicate_begin != duplicate_end) {
                error << ", " << *duplicate_begin++;
            }
            return false;
        }
        return true;
    }

    [[nodiscard]] std::vector<std::string> shellExtensions() const { return shell_extensions_.get(COMBINE_FUNCTION); }
    [[nodiscard]] std::vector<std::string> pythonExtensions() const { return python_extensions_.get(COMBINE_FUNCTION); }
    [[nodiscard]] std::vector<std::string> luaExtensions() const { return lua_extensions_.get(COMBINE_FUNCTION); }
    [[nodiscard]] std::vector<std::string> cExtensions() const { return c_extensions_.get(COMBINE_FUNCTION); }
    [[nodiscard]] std::vector<std::string> cppExtensions() const { return cpp_extensions_.get(COMBINE_FUNCTION); }

private:
    [[nodiscard]] static constexpr std::vector<std::string> defaultShellExtensions() { return { ".sh", ".bash", ".zsh", "" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultPythonExtensions() { return { ".py", ".py3" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultLuaExtensions() { return { ".lua" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultCExtensions() { return { ".so" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultCppExtensions() { return { }; }

    static constexpr auto COMBINE_FUNCTION = [](auto&& prev, auto&& next) { return appendVector(prev, next); };

private:
    ConfigurationOption<std::vector<std::string>> shell_extensions_ { defaultShellExtensions() };
    ConfigurationOption<std::vector<std::string>> python_extensions_ { defaultPythonExtensions() };
    ConfigurationOption<std::vector<std::string>> lua_extensions_ { defaultLuaExtensions() };
    ConfigurationOption<std::vector<std::string>> c_extensions_ { defaultCExtensions() };
    ConfigurationOption<std::vector<std::string>> cpp_extensions_ { defaultCppExtensions() };
};

#endif // QQ_CONFIGURATION_EXTENSIONS_H
