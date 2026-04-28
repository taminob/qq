#ifndef QQ_SCRIPT_STORAGE_H
#define QQ_SCRIPT_STORAGE_H

#include "configuration/configuration.h"
#include "matcher/script_matcher.h"
#include "script.h"
#include "ui.h"

#include <filesystem>
#include <vector>

class ScriptStorage {
public:
    explicit ScriptStorage(Ui* ui, Configuration* config)
        : ui_ { ui }
        , config_ { config }
    {
        assert(ui_ != nullptr);
        assert(config_ != nullptr);
    }

    /**
     * Store given script in storage of application.
     *
     * @return path to file in storage on success, otherwise error string
     */
    std::expected<std::filesystem::path, std::string> store(const Script& script, bool store_symlink = false)
    {
        auto&& path = script.pluginPath();
        auto&& name = path.filename();
        if (!std::filesystem::exists(path)) {
            // source does not exist
            return std::unexpected { "source file does not exist" };
        }

        // use first storage path that is writable
        for (auto&& storage_path : config_->getStoragePaths()) {
            // handle duplicate filenames
            auto&& target_path = findNextAvailableFilePath(storage_path / SCRIPT_STORAGE_DIRECTORY / name);
            ui_->debug() << "Trying to store at " << target_path << "...\n";

            std::error_code error;
            std::filesystem::create_directories(target_path.parent_path(), error);
            if (error) {
                // directory does not exist and cannot be created
                continue;
            }
            if (store_symlink) {
                std::filesystem::create_symlink(path, target_path, error);
            } else {
                std::filesystem::copy(path, target_path, std::filesystem::copy_options::recursive, error);
            }
            if (!error) {
                // copy was successful
                return target_path;
            }
            ui_->debug() << "Failed to store at " << target_path << ": [" << error.category().name() << "] " << error.message() << "\n";
        }
        // no storage path worked
        return std::unexpected { "no writable storage path available" };
    }

    template <typename Matcher>
        requires IsScriptMatcher<Matcher>
    [[nodiscard]] std::vector<std::filesystem::path> search(std::string_view search_pattern) const
    {
        Matcher matcher { config_ };
        return matcher.match(config_->getStoragePaths(), search_pattern);
    }

private:
    static constexpr std::string_view SCRIPT_STORAGE_DIRECTORY = "storage";

private:
    Ui* ui_;
    Configuration* config_; // TODO: decide if stored as reference (e.g. for Configuration updates)
};

#endif // QQ_SCRIPT_STORAGE_H
