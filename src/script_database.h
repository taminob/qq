#ifndef QQ_SCRIPT_DATABASE_H
#define QQ_SCRIPT_DATABASE_H

#include "configuration/configuration.h"
#include "plugin_type.h"
#include "script.h"
#include "ui.h"
#include "utils/filesystem_utils.h"
#include "utils/reference_utils.h"
#include "utils/string_utils.h"

#include <filesystem>
#include <ranges>
#include <set>
#include <string>
#include <vector>

#include <boost/json.hpp>

/**
 * Database that consists of a collection of JSON files.
 * It contains metadata about the stored scripts.
 */
class ScriptDatabase {
public:
    explicit ScriptDatabase(Ui* ui, Configuration* config)
        : ui_ { ui }
        , config_ { config }
    {
        assert(ui_ != nullptr);
        assert(config_ != nullptr);

        iterateFiles(config_->getStoragePaths() | std::views::transform([](auto&& path) { return path / DATABASE_DIRECTORY; }), [this](auto&& file_path) {
            if (auto read_result = readDatabase(file_path); !read_result) {
                ui_->error() << "Failed to read database at " << file_path << ": " << read_result.error() << "\n";
            }
        });
    }

    /**
     * Add script to database.
     *
     * @return the path to the database file on success, otherwise std::nullopt
     */
    std::optional<std::filesystem::path> add(const Script& script)
    {
        if (!indexScript(script)) {
            ui_->debug() << "Duplicate script detected - adding it to database failed\n";
            return std::nullopt;
        }
        return writeToDatabase(script);
    }

    /**
     * Update script in database.
     *
     * @return true on success,
     *         false if the script was not found in database or no change was applied
     */
    template <typename Func>
        requires std::invocable<Func, Script&>
    bool update(const Script& script, Func&& update_function)
    {
        auto it = scripts_.find(script);
        if (it == scripts_.end()) {
            // script does not exist in database
            return false;
        }
        // create copy of script before applying user change
        const auto script_copy = *it; // NOLINT(performance-unnecessary-copy-initialization)
        std::invoke(std::forward<Func>(update_function), *it);
        if (script_copy == *it) {
            // no change was applied to script
            return false;
        }

        if (it->functionName() != script_copy.functionName()) {
            eraseMultimapElement(by_name_, script_copy.functionName(), [&](auto&& value) { return &value.get() == &*it; });
            by_name_.emplace(it->functionName(), std::ref(*it));
        }
        if (it->tags() != script_copy.tags()) {
            // remove all old tags from index
            for (auto&& old_tag : script_copy.tags()) {
                eraseMultimapElement(by_tag_, old_tag, [&](auto&& value) { return &value.get() == &*it; });
            }
            // add all new tags
            for (auto&& new_tag : it->tags()) {
                by_tag_.emplace(new_tag, std::ref(*it));
            }
        }
        return true;
    }

    [[nodiscard]] std::vector<Script> searchByName(std::string_view name, std::size_t candidate_limit = std::numeric_limits<std::size_t>::max()) const
    {
        auto [begin, end] = by_name_.equal_range(name);
        auto&& candidates = std::ranges::subrange(begin, end)
            | std::views::transform([](auto&& key_value_pair) {
                  return key_value_pair.second;
              });
        return toVector<Script>(candidates, candidate_limit);
        // if (candidate_limit != std::numeric_limits<std::size_t>::max()) {
        //     candidates = candidates | std::views::take(candidate_limit);
        // }
        // return candidates | std::ranges::to<std::vector<Script>>();
    }

    [[nodiscard]] std::vector<Script> searchByTags(const std::vector<std::string>& tags, std::size_t candidate_limit = std::numeric_limits<std::size_t>::max()) const
    {
        // count for each script how many tags match
        std::map<std::reference_wrapper<const Script>, std::size_t, ReferenceWrapperLess<const Script>> candidates;
        for (auto&& tag : tags) {
            auto [begin, end] = by_tag_.equal_range(tag);
            for (auto it = begin; it != end; ++it) {
                auto [candidate_it, is_inserted] = candidates.insert(std::make_pair(std::ref(it->second), 0));
                if (!is_inserted) {
                    ++candidate_it->second;
                }
            }
        }

        // sort candidates by how many tags match
        auto result = candidates | std::views::keys | std::ranges::to<std::vector<std::reference_wrapper<const Script>>>();
        std::ranges::sort(result, [&](auto&& lhs, auto&& rhs) {
            auto lhs_it = candidates.find(std::ref(lhs));
            auto rhs_it = candidates.find(std::ref(rhs));
            assert(lhs_it != candidates.end());
            assert(rhs_it != candidates.end());
            return lhs_it->second < rhs_it->second;
        });
        return toVector<Script>(result, candidate_limit);
    }

private:
    bool indexScript(const Script& script)
    {
        auto [it, is_inserted] = scripts_.insert(script);
        if (!is_inserted) {
            // script already exists
            return false;
        }
        by_name_.emplace(script.functionName(), std::ref(*it));
        for (auto&& tag : script.tags()) {
            by_tag_.emplace(tag, std::ref(*it));
        }
        return true;
    }

    /**
     * Read given JSON database file.
     *
     * JSON Format:
     * {
     *   "scripts": [
     *      {
     *        "path": "...",
     *        "type": "...",
     *        "name": "...",
     *        "tags": [ ... ]
     *      },
     *      ...
     *   ]
     * }
     */
    [[nodiscard]] std::expected<void, std::string> readDatabase(const std::filesystem::path& file_path)
    {
        ui_->debug() << "Reading database at " << file_path << "...\n";

        std::size_t num_scripts { 0 };
        auto database = openDatabaseFile(file_path);
        if (!database) {
            return std::unexpected { database.error() };
        }
        if (auto* scripts = database->if_contains("scripts"); scripts != nullptr && scripts->is_array()) {
            for (auto script_value : scripts->as_array()) {
                if (auto* script_object = script_value.if_object()) {
                    auto script = parse(*script_object);
                    if (script.pluginPath().empty() || script.functionName().empty()) {
                        ui_->debug() << "Read incomplete database entry from " << file_path << '\n';
                    }
                    indexScript(script);
                    databases_.insert_or_assign(std::ref(script), file_path);
                    ++num_scripts;
                }
            }
        }
        ui_->debug() << "Read " << num_scripts << " scripts from " << file_path << "\n";
        return { };
    }

    [[nodiscard]] std::optional<std::filesystem::path> writeToDatabase(const Script& script)
    {
        auto database_path = findDatabaseFilePath(script);
        std::error_code error;
        std::filesystem::create_directories(database_path.parent_path(), error);
        if (error) {
            ui_->error() << "Unable to create database directory " << database_path.parent_path() << ": [" << error.category().name() << "] " << error.message() << '\n';
            return std::nullopt;
        }
        boost::json::object database;

        if (std::filesystem::exists(database_path)) {
            // database file already exists, read database first to add new script to it
            if (auto old_database = openDatabaseFile(database_path)) {
                database = *old_database;
            } else {
                // database corrupt, create new database file to allow user to fix this issue
                ui_->error() << "Database at " << database_path << " is corrupted\n";
                database_path = findNextAvailableFilePath(database_path);
            }
        }

        boost::json::array scripts;
        if (auto* old_scripts = database.if_contains("scripts"); old_scripts != nullptr && old_scripts->is_array()) {
            // existing database already contains scripts
            scripts = old_scripts->as_array();
        }
        // TODO: prevent duplicates
        scripts.push_back(serialize(script));
        database["scripts"] = scripts;

        databases_.insert_or_assign(std::ref(script), database_path);
        if (!dumpDatabaseFile(database_path, database)) {
            ui_->debug() << "Failed to write script to database file " << database_path << "\n";
            return std::nullopt;
        }
        return database_path;
    }

    /**
     * @return database JSON object on success, otherwise an error string
     */
    [[nodiscard]] static std::expected<boost::json::object, std::string> openDatabaseFile(const std::filesystem::path& database_path)
    {
        std::ifstream in_file { database_path };
        if (!in_file) {
            return std::unexpected { "unable to open file" };
        }
        boost::system::error_code error;
        auto parse_result = boost::json::parse(in_file, error);
        if (error) {
            return std::unexpected { std::string { "unable to parse file: " } + error.message() };
        }
        if (auto* database = parse_result.if_object()) {
            return *database;
        }
        return std::unexpected { "invalid database structure" };
    }

    [[nodiscard]] static bool dumpDatabaseFile(const std::filesystem::path& database_path, const boost::json::object& data)
    {
        std::ofstream out_file { database_path };
        if (!out_file) {
            return false;
        }
        out_file << data;
        return static_cast<bool>(out_file);
    }

    std::filesystem::path findDatabaseFilePath(const Script& script)
    {
        if (auto it = databases_.find(std::ref(script)); it != databases_.end()) {
            return it->second;
        }

        auto&& storage_paths = config_->getStoragePaths();
        for (auto&& storage_path : storage_paths) {
            // find storage path that contains the script
            if (isFileInDirectory(storage_path, script.pluginPath())) {
                return storage_path / DATABASE_DIRECTORY / DEFAULT_DATABASE_NAME;
            }
        }
        if (storage_paths.empty()) {
            return { };
        }
        return storage_paths.front() / DATABASE_DIRECTORY / DEFAULT_DATABASE_NAME;
    }

    [[nodiscard]] static Script parse(boost::json::object object)
    {
        auto read_property = [&](std::string_view key) -> std::string {
            if (auto* value = object.if_contains(key); value != nullptr && value->is_string()) {
                return std::string { value->as_string() };
            }
            return std::string { };
        };
        auto plugin_path = read_property("path");
        auto plugin_type = fromString<PluginType>(read_property("type")).value_or(PluginType::unknown);
        auto function_name = read_property("name");

        std::vector<std::string> tags;
        if (auto* value = object.if_contains("tags")) {
            if (auto* array_value = value->if_array()) {
                for (auto tag : *array_value) {
                    if (tag.is_string()) {
                        tags.push_back(std::string { tag.as_string() });
                    }
                }
            }
        }

        return Script { plugin_path, plugin_type, function_name, tags };
    }

    static boost::json::object serialize(const Script& script)
    {
        boost::json::object json;
        json["path"] = script.pluginPath().string();
        json["type"] = toString(script.pluginType());
        json["name"] = script.functionName();
        json["tags"] = boost::json::value_from(script.tags());
        return json;
    }

private:
    static constexpr std::string_view DATABASE_DIRECTORY = "index";
    static constexpr std::string_view DEFAULT_DATABASE_NAME = "index.json";

private:
    Ui* ui_;
    Configuration* config_;

    std::set<Script> scripts_;
    std::unordered_multimap<std::string, std::reference_wrapper<const Script>, StringHash, std::equal_to<>> by_tag_;
    std::unordered_multimap<std::string, std::reference_wrapper<const Script>, StringHash, std::equal_to<>> by_name_;

    // TODO? most database paths are probably the same, store in separate set?
    std::unordered_map<std::reference_wrapper<const Script>, std::filesystem::path, ReferenceWrapperHash<const Script>, ReferenceWrapperEqualTo<const Script>> databases_;
};

#endif // QQ_SCRIPT_DATABASE_H
