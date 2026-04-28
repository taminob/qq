#ifndef QQ_CONFIGURATION_H
#define QQ_CONFIGURATION_H

#include "display.h"
#include "extensions.h"
#include "paths.h"

#include <filesystem>
#include <string>
#include <vector>

#include <ppplugin/detail/scope_guard.h> // TODO: do not use detail namespace
#include <ppplugin/plugin.h>

class Configuration {
public:
    explicit Configuration(Ui* ui)
        : ui_ { ui }
    {
    }

    void reload()
    {
        iterateFiles(getConfigPaths(), [this](auto&& file_path) {
            if (!runConfigFile(file_path)) {
                ui_->error() << "Failed to run config file " << file_path << '\n';
            }
        });
    }

    [[nodiscard]] std::vector<std::string> getShellExtensions() const
    {
        return extensions_.shellExtensions();
    }
    [[nodiscard]] std::vector<std::string> getPythonExtensions() const
    {
        return extensions_.pythonExtensions();
    }
    [[nodiscard]] std::vector<std::string> getLuaExtensions() const
    {
        return extensions_.luaExtensions();
    }
    [[nodiscard]] std::vector<std::string> getCExtensions() const
    {
        return extensions_.cExtensions();
    }
    [[nodiscard]] std::vector<std::string> getCppExtensions() const
    {
        return extensions_.cppExtensions();
    }

    [[nodiscard]] std::vector<std::filesystem::path> getStoragePaths() const
    {
        return paths_.storagePaths();
    }
    [[nodiscard]] std::vector<std::filesystem::path> getConfigPaths() const
    {
        return paths_.configPaths();
    }
    [[nodiscard]] const std::filesystem::path& getShellExecutablePath() const
    {
        return paths_.shellExecutablePath();
    }

    [[nodiscard]] std::size_t getScreenWidth() const
    {
        return display_.screenWidth();
    }
    [[nodiscard]] std::size_t getCandidateDisplayLimit() const
    {
        return display_.candidateDisplayLimit();
    }

private:
    bool runConfigFile(const std::filesystem::path& config_path)
    {
        ppplugin::detail::ScopeGuard restore_guard { [this, copy = *this]() { *this = copy; } };

        ui_->debug() << "Trying to load config file " << config_path << "...\n";
        auto loaded_plugin = ppplugin::ShellPlugin::load(config_path);
        if (!loaded_plugin) {
            ui_->debug() << "Unable to load config file " << config_path << " (" << loaded_plugin.error() << ")\n";
            return false;
        }
        ppplugin::Plugin plugin { std::move(loaded_plugin.value()) };

        std::ignore = plugin.call<void>("load");

        extensions_.read(plugin);
        paths_.read(plugin);
        display_.read(plugin);

        std::ignore = plugin.call<void>("loaded");
        if (!plugin) {
            ui_->debug() << "Error while loading config file " << config_path << '\n';
            return false;
        }
        normalize();
        if (verify()) {
            std::ignore = plugin.call<void>("verify_success");
        } else {
            std::ignore = plugin.call<void>("verify_error");
            ui_->debug() << "Reverting config file " << config_path << " - invalid configuration detected\n";
            return false;
        }
        ui_->debug() << "Finished reading config from " << config_path << '\n';
        restore_guard.cancel();
        return true;
    }

    /**
     * Simplify and normalize current configuration.
     */
    void normalize()
    {
        extensions_.normalize();
        paths_.normalize();
        display_.normalize();
    }

    /**
     * @note requires that the current configuration is normalized
     */
    [[nodiscard]] bool verify() const
    {
        auto success { true };
        assert(ui_);
        success = success && extensions_.verify(*ui_);
        success = success && paths_.verify(*ui_);
        success = success && display_.verify(*ui_);
        return success;
    }

private:
    Ui* ui_;

    ExtensionsConfiguration extensions_;
    PathsConfiguration paths_;
    DisplayConfiguration display_;
};

#endif // QQ_CONFIGURATION_H
