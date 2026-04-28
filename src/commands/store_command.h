#ifndef QQ_COMMANDS_STORE_COMMAND_H
#define QQ_COMMANDS_STORE_COMMAND_H

#include "commands/command_registry.h"
#include "script_database.h"
#include "script_storage.h"
#include "ui.h"

#include <string>

#include <boost/program_options.hpp>

//   - --name
//   - --description
//   - --tags
//   - --autotag
//   - --autorun (automatically run closest match)
//   - --noconfirm
//   - --class / --category (?)
//   - --command (positional?)
class StoreCommand {
public:
    explicit StoreCommand(Ui* ui, Configuration* config, boost::program_options::variables_map arguments)
        : ui_ { ui }
        , config_ { config }
        , arguments_ { std::move(arguments) }
    {
        assert(ui_ != nullptr);
        assert(config_ != nullptr);
    }

    bool run()
    {
        if (!validateArguments()) {
            return false;
        }

        ScriptStorage storage { ui_, config_ };
        ScriptDatabase database { ui_, config_ };

        auto script_name = arguments_.at(NAME).as<std::string>();
        auto tags = arguments_.at(TAGS).as<std::vector<std::string>>();
        if (arguments_.at(AUTOTAG).as<bool>()) {
            // TODO
        }
        auto type = arguments_.at(TYPE).as<PluginType>();
        auto store_symlink = arguments_.at(SYMLINK).as<bool>();
        std::filesystem::path script_path;
        if (arguments_.at(SCRIPT_FILE).as<bool>()) {
            script_path = arguments_.at(SCRIPT).as<std::string>();
        } else {
            auto script_content = arguments_.at(SCRIPT).as<std::string>();
            if (auto script = embedInFunction(type, script_name, script_content)) {
                script_path = temporaryFile(script_name + ".script");
                std::ofstream { script_path } << *script;
            } else {
                ui_->error() << "Unable to create '" << toString(type) << "' function\n";
                return false;
            }
        }
        Script script {
            script_path,
            type,
            script_name, // TODO? is script_name == function_name?
            tags,
        };
        auto store_result = storage.store(script, store_symlink);
        if (!store_result) {
            ui_->error() << "Unable to add script " << script.pluginPath() << " to storage: " << store_result.error() << "\n";
            return false;
        }
        script.updatePath(store_result.value());

        ui_->output() << "Script stored at " << store_result.value() << "\n";
        if (auto database_path = database.add(script)) {
            ui_->debug() << "Saved script metadata in database at " << *database_path << '\n';
            return true;
        }
        ui_->debug() << "Failed to add script to database\n";
        return false;
    }

    static constexpr std::string id()
    {
        return "store";
    }

    static constexpr void fillCommandLineOptions(CommandOptions& command_options)
    {
        namespace po = boost::program_options;

        // clang-format off
        command_options.options.add_options()
            (NAME, po::value<std::string>()->value_name("name")->required(), "Name of script")
            (TAGS, po::value<std::vector<std::string>>()->value_name("tags...")->multitoken()->composing()->default_value({}, ""), "Tags of script")
            (AUTOTAG, po::bool_switch(), "Automatically determine tags of script")
            (TYPE, po::value<PluginType>()->value_name("type")->default_value(PluginType::shell), "Type of script [shell, lua, python, c, cpp, file_extension]")
            (SYMLINK, po::bool_switch(), "Store file as symlink to original location")
            (SCRIPT_FILE, po::bool_switch(), "Interpret script as path to script file");
        // clang-format on

        // clang-format off
        command_options.hiddenOptions.add_options()
            (SCRIPT, po::value<std::string>(), "Script to be stored")
            (SCRIPT_ARGS, po::value<std::vector<std::string>>()->multitoken()->composing()->default_value({}, ""), "Arguments of script");
        // clang-format on

        command_options.positionalOptions.add(SCRIPT, 1);
        command_options.positionalOptions.add(SCRIPT_ARGS, -1);
    }

private:
    // TODO? make it static and verify in CommandLineParser?
    [[nodiscard]] bool validateArguments()
    {
        if (!arguments_.contains(SCRIPT)) {
            ui_->error() << "A script is required\n";
            return false;
        }
        if (arguments_.at(SYMLINK).as<bool>() && !arguments_.at(SCRIPT_FILE).as<bool>()) {
            ui_->error() << "--" << SYMLINK << " requires --" << SCRIPT_FILE << '\n';
            return false;
        }
        if (arguments_.at(SCRIPT_FILE).as<bool>() && arguments_.contains(SCRIPT_ARGS) && !arguments_.at(SCRIPT_ARGS).as<std::vector<std::string>>().empty()) {
            ui_->error() << "--" << SCRIPT_FILE << " requires a single script argument\n";
            return false;
        }
        return true;
    }

private:
    static constexpr const char* NAME = "name";
    static constexpr const char* TAGS = "tags";
    static constexpr const char* AUTOTAG = "autotag";
    static constexpr const char* TYPE = "type";
    static constexpr const char* SYMLINK = "symlink";
    static constexpr const char* SCRIPT_FILE = "file";
    static constexpr const char* SCRIPT = "script";
    static constexpr const char* SCRIPT_ARGS = "script_args";

private:
    Ui* ui_;
    Configuration* config_;

    boost::program_options::variables_map arguments_;
};

#endif // QQ_COMMANDS_STORE_COMMAND_H
