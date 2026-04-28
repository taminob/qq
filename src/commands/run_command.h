#ifndef QQ_COMMANDS_RUN_COMMAND_H
#define QQ_COMMANDS_RUN_COMMAND_H

#include "commands/command_registry.h"
#include "script_database.h"
#include "script_executor.h"
#include "script_storage.h"
#include "ui.h"

#include <string>

#include <boost/program_options.hpp>

//   - --name
//   - --tags
//   - --autotag
//   - --autorun (automatically run closest match)
//   - --noconfirm
//   - --class / --category (?)
//   - --command (positional?)
class RunCommand {
public:
    explicit RunCommand(Ui* ui, Configuration* config, boost::program_options::variables_map arguments)
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

        auto candidates = findScripts(config_->getCandidateDisplayLimit());
        if (candidates.empty()) {
            return false;
        }

        auto choice = ui_->userChoice(candidates | std::views::transform(SCRIPT_TO_STRING));
        auto&& script = candidates.at(choice);

        if (arguments_.at(PRINT).as<bool>()) {
            auto&& output = ui_->output();
            output << script.pluginPath() << ":\n"
                   << std::string(config_->getScreenWidth(), '=') << '\n';
            return printFile(output, script.pluginPath());
        }
        return runScript(script);
    }

    static constexpr std::string id()
    {
        return "run";
    }

    static constexpr void fillCommandLineOptions(CommandOptions& command_options)
    {
        namespace po = boost::program_options;

        // clang-format off
        command_options.options.add_options()
            (MATCHER, po::value<std::string>(), "Name of script")
            (GREP, po::bool_switch(), "Grep for match based on name in scripts")
            (TAGS, po::bool_switch(), "Match based on tags")
            (PRINT, po::bool_switch(), "Print command instead of running it");
        // clang-format on

        // clang-format off
        command_options.hiddenOptions.add_options()
            (PATTERN, po::value<std::vector<std::string>>()->multitoken()->default_value({}, ""), "Search pattern");
        // clang-format on

        command_options.positionalOptions.add(PATTERN, -1);
    }

private:
    [[nodiscard]] bool validateArguments()
    {
        if (arguments_.at(GREP).as<bool>() && arguments_.at(TAGS).as<bool>()) {
            ui_->error() << "--" << GREP << " and --" << TAGS << " are mutually exclusive!\n";
            return false;
        }
        if (arguments_.at(PATTERN).as<std::vector<std::string>>().empty()) {
            ui_->error() << "At least one search pattern required!\n";
            return false;
        }
        return true;
    }

    [[nodiscard]] std::vector<Script> findScripts(std::size_t candidate_limit)
    {
        auto&& pattern = arguments_.at(PATTERN).as<std::vector<std::string>>();
        if (arguments_.at(GREP).as<bool>()) {
            ScriptStorage storage { ui_, config_ };
            auto&& name = pattern.front();
            auto&& candidates = storage.search<FunctionNameScriptMatcher>(name);
            if (candidates.empty()) {
                ui_->error() << "Unable to find match in scripts for name '" << name << "'\n";
            }
            return candidates
                | std::views::take(candidate_limit)
                | std::views::transform([&](auto&& path) {
                      return Script {
                          path,
                          detectPluginType(*config_, path),
                          name,
                      };
                  })
                | std::ranges::to<std::vector<Script>>();
        }
        if (arguments_.at(TAGS).as<bool>()) {
            ScriptDatabase database { ui_, config_ };
            auto&& candidates = database.searchByTags(pattern, candidate_limit);
            if (candidates.empty()) {
                auto&& error = ui_->error();
                error << "Unable to find match for tags '";
                for (auto&& tag : std::views::join_with(pattern, "', '")) {
                    error << tag;
                }
                error << "'\n";
            }
            return candidates;
        }

        ScriptDatabase database { ui_, config_ };
        auto&& name = pattern.front();
        auto&& candidates = database.searchByName(name, candidate_limit);
        if (candidates.empty()) {
            ui_->error() << "Unable to find match in database for name '" << name << "'\n";
        }
        return candidates;
    }

    [[nodiscard]] bool runScript(const Script& script)
    {
        ScriptExecutor executor { ui_, config_ };

        if (!executor.load(script)) {
            return false;
        }
        if (auto error = executor.execute().error) {
            ui_->error() << "Error: " << *error << '\n';
            return false;
        }
        return true;
    }

private:
    // TODO? use po::value<>(ptr) instead to directly fill members?
    static constexpr const char* MATCHER = "matcher";
    static constexpr const char* GREP = "name";
    static constexpr const char* TAGS = "tags";
    static constexpr const char* PRINT = "print";
    static constexpr const char* PATTERN = "pattern";

    static constexpr auto SCRIPT_TO_STRING = [](auto&& script) {
        auto result = "'" + script.functionName() + "' in [" + script.pluginPath().string() + "]:" + std::string { toString(script.pluginType()) } + " (";
        for (auto&& tag : std::views::join_with(script.tags(), ", ")) {
            result += tag;
        }
        result += ")";
        return result;
    };

private:
    Ui* ui_;
    Configuration* config_;
    boost::program_options::variables_map arguments_;
};

#endif // QQ_COMMANDS_RUN_COMMAND_H
