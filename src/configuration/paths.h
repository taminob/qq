#ifndef QQ_CONFIGURATION_PATHS_H
#define QQ_CONFIGURATION_PATHS_H

#include "configuration_option.h"
#include "ui.h"
#include "utils/filesystem_utils.h"
#include "utils/vector_utils.h"

#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include <ppplugin/plugin.h>

class PathsConfiguration {
public:
    PathsConfiguration() = default;

    void read(ppplugin::Plugin& plugin)
    {
        auto new_storage_paths = plugin.global<std::vector<std::string>>("STORAGE_PATHS").valueOr(std::vector<std::string> { })
            | std::ranges::to<std::vector<std::filesystem::path>>();
        if (!new_storage_paths.empty()) {
            storage_paths_.add(ConfigurationLayer::file, new_storage_paths, COMBINE_VECTORS);
        }

        auto new_config_paths = plugin.global<std::vector<std::string>>("CONFIG_PATHS").valueOr(std::vector<std::string> { })
            | std::ranges::to<std::vector<std::filesystem::path>>();
        if (!new_config_paths.empty()) {
            config_paths_.add(ConfigurationLayer::file, new_config_paths, COMBINE_VECTORS);
        }
auto new_value = plugin.global<std::string>("SHELL_EXECUTABLE");
        if (new_value) {
            shell_executable_path_.overwrite(ConfigurationLayer::file, *new_value);
			std::cerr << "SUCCESSFULLY read shell exe: " << *new_value << '\n';
		} else {
			std::cerr << "FAILED to read shell exe: " << new_value.error() << '\n';
		}
    }

    void normalize()
    {
        storage_paths_.forEachLayer(CANONICALIZE_PATHS);
        storage_paths_.removeDuplicatesAcrossLayers();

        config_paths_.forEachLayer(CANONICALIZE_PATHS);
        config_paths_.removeDuplicatesAcrossLayers();

        shell_executable_path_.forEachLayer(CANONICALIZE_PATH);
    }

    /**
     *
     */
    [[nodiscard]] bool verify(Ui& ui) const
    {
        if (!std::filesystem::exists(shell_executable_path_.get())) {
            ui.error() << "Configured shell executable " << shell_executable_path_.get() << " does not exist\n";
            return false;
        }
        return true;
    }

    [[nodiscard]] std::vector<std::filesystem::path> storagePaths() const
    {
        return storage_paths_.get(COMBINE_VECTORS);
    }

    [[nodiscard]] std::vector<std::filesystem::path> configPaths() const
    {
        return config_paths_.get(COMBINE_VECTORS);
    }

    [[nodiscard]] const std::filesystem::path& shellExecutablePath() const
    {
        return shell_executable_path_.get();
    }

private:
    [[nodiscard]] static std::vector<std::filesystem::path> defaultStoragePaths() { return { defaultAppDataDirectory() }; }
    [[nodiscard]] static std::vector<std::filesystem::path> defaultConfigPaths() { return { defaultAppConfigDirectory() }; }
    [[nodiscard]] static std::filesystem::path defaultShelExecutablePath() { return "/bin/sh"; }

    static constexpr auto COMBINE_VECTORS = [](auto prev, auto&& next) { return appendVector(prev, next); };
    static constexpr auto CANONICALIZE_PATH = [](auto&& /*layer*/, auto& value) {
        value = std::filesystem::canonical(value);
    };
    static constexpr auto CANONICALIZE_PATHS = [](auto&& layer, auto& value) {
        for (auto& path : value) {
            CANONICALIZE_PATH(layer, path);
        }
    };

private:
    ConfigurationOption<std::vector<std::filesystem::path>> storage_paths_ { defaultStoragePaths() };
    ConfigurationOption<std::vector<std::filesystem::path>> config_paths_ { defaultConfigPaths() };
    ConfigurationOption<std::filesystem::path> shell_executable_path_ { defaultShelExecutablePath() };
};

#endif // QQ_CONFIGURATION_PATHS_H
