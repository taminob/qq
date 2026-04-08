#ifndef QQ_CONFIGURATION_H
#define QQ_CONFIGURATION_H

#include "utils.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <ppplugin/detail/scope_guard.h> // TODO
#include <ppplugin/plugin.h>

// layers:
//   - default
//   - configuration file
//   - cli arguments
//   - runtime override
class Configuration {
public:
    Configuration() = default;

    bool runConfigFile(const std::filesystem::path& config_path)
    {
        ppplugin::detail::ScopeGuard restore_guard { [this, copy = *this]() { *this = copy; } };

        auto plugin = ppplugin::ShellPlugin::load(config_path);
        if (!plugin) {
            return false;
        }

        std::ignore = plugin->call<void>("load");
        // TODO: merge with current configuration instead of replacing it
        shell_extensions_ = plugin->global<std::vector<std::string>>("SHELL_EXTENSIONS").valueOr(std::vector<std::string> { });
        python_extensions_ = plugin->global<std::vector<std::string>>("PYTHON_EXTENSIONS").valueOr(std::vector<std::string> { });
        lua_extensions_ = plugin->global<std::vector<std::string>>("LUA_EXTENSIONS").valueOr(std::vector<std::string> { });
        c_extensions_ = plugin->global<std::vector<std::string>>("C_EXTENSIONS").valueOr(std::vector<std::string> { });
        cpp_extensions_ = plugin->global<std::vector<std::string>>("CPP_EXTENSIONS").valueOr(std::vector<std::string> { });
        std::ignore = plugin->call<void>("loaded");
        if (!plugin) {
            return false;
        }
        normalize(); // TODO? restore to previous state in case of verification failure?
        if (verify()) {
            std::ignore = plugin->call<void>("verified");
        } else {
            std::ignore = plugin->call<void>("verify_failed");
            return false;
        }
        restore_guard.cancel();
        return true;
    }

    [[nodiscard]] const std::vector<std::string>& getShellExtensions() const
    {
        return shell_extensions_;
    }
    [[nodiscard]] const std::vector<std::string>& getPythonExtensions() const
    {
        return python_extensions_;
    }
    [[nodiscard]] const std::vector<std::string>& getLuaExtensions() const
    {
        return lua_extensions_;
    }
    [[nodiscard]] const std::vector<std::string>& getCExtensions() const
    {
        return c_extensions_;
    }
    [[nodiscard]] const std::vector<std::string>& getCppExtensions() const
    {
        return cpp_extensions_;
    }

private:
    /**
     * Simplify and normalize current configuration.
     */
    void normalize()
    {
        auto sort_and_remove_duplicates = [](auto&& range) {
            std::ranges::sort(range);
            auto [duplicate_begin, duplicate_end] = std::ranges::unique(range);
            range.erase(duplicate_begin, duplicate_end);
        };
        sort_and_remove_duplicates(shell_extensions_);
        sort_and_remove_duplicates(python_extensions_);
        sort_and_remove_duplicates(lua_extensions_);
        sort_and_remove_duplicates(c_extensions_);
        sort_and_remove_duplicates(cpp_extensions_);
    }

    /**
     * @note requires that the current configuration is normalized
     */
    [[nodiscard]] bool verify(std::optional<std::reference_wrapper<std::ostream>> out = std::nullopt) const
    {
        // an extension may only appear once for all plugin types
        std::vector<std::string> all_extensions;
        appendVector(all_extensions, shell_extensions_);
        appendVector(all_extensions, python_extensions_);
        appendVector(all_extensions, lua_extensions_);
        appendVector(all_extensions, c_extensions_);
        appendVector(all_extensions, cpp_extensions_);
        auto [duplicate_begin, duplicate_end] = std::ranges::unique(all_extensions);
        if (duplicate_begin != duplicate_end) {
            if (out) {
                (*out).get() << "Found duplicates in extensions for plugin type detection: " << *duplicate_begin++;
                while (duplicate_begin != duplicate_end) {
                    (*out).get() << ", " << *duplicate_begin++;
                }
            }
            return false;
        }
        return true;
    }

private:
    [[nodiscard]] static constexpr std::vector<std::string> defaultShellExtensions() { return { ".sh", ".bash", ".zsh", "" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultPythonExtensions() { return { ".py", ".py3" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultLuaExtensions() { return { ".lua" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultCExtensions() { return { ".so" }; }
    [[nodiscard]] static constexpr std::vector<std::string> defaultCppExtensions() { return { }; }

private:
    std::vector<std::string> shell_extensions_;
    std::vector<std::string> python_extensions_;
    std::vector<std::string> lua_extensions_;
    std::vector<std::string> c_extensions_;
    std::vector<std::string> cpp_extensions_;
};

#endif // QQ_CONFIGURATION_H
