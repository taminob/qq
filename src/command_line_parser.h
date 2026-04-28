#ifndef QQ_COMMAND_LINE_PARSER_H
#define QQ_COMMAND_LINE_PARSER_H

#include "commands/command_registry.h"
#include "ui.h"

#include <iostream>
#include <span>

#include <boost/program_options.hpp>

// Commands
//   - run (default?)
//   - save (store command)
//   - store (store file)
//   - edit
//   - delete
//   - list
//   - search
//   - register (?, allow definition of new commands)
//   - scan (?, check config directory for changes)
//   - update-autocomplete (for shell tab-completion)
//   - generate: generate
//   - update-index (scan directories and update database)
//
// Command Options:
//   - --name
//   - --tags
//   - --autotag
//   - --autorun (automatically run closest match)
//   - --noconfirm
//   - --class / --category (?)
//   - --command (positional?)
//   - --llm (use LLM matcher)
//   - --symlink (store symlink to given file)

class CommandLineParser {
public:
    struct CommandLineArguments {
        boost::program_options::variables_map globalArguments;
        std::string command;
        boost::program_options::variables_map commandArguments;
    };

public:
    explicit CommandLineParser(Ui* ui, CommandRegistry command_registry)
        : ui_ { ui }
        , command_registry_ { std::move(command_registry) }
    {
        setupGlobalOptions();
        setupPositionalOptions();
    }

    /**
     * Parse given command-line arguments.
     *
     * @return parsed arguments; if std::nullopt is returned, the program is expected to exit
     */
    [[nodiscard]] std::optional<CommandLineArguments> parse(std::span<char*> args)
    {
        namespace po = boost::program_options;
        try {
            CommandLineArguments result;

            // TODO? when trying to run "qq store --name abc --tags xyz -- 'echo qq'", the first parsing pass
            //       will remove the "--" from the arguments, causing 'echo qq' to be interpreted as an additional
            //       argument for --tags; this can be prevented by using "-- --" which is unintuitive; insert second
            //       "--" automatically?
            po::options_description all_global_options;
            all_global_options.add(global_options_);
            all_global_options.add(hidden_global_options_);
            po::parsed_options global_parsed = po::command_line_parser { static_cast<int>(args.size()), args.data() }
                                                   .options(all_global_options)
                                                   .positional(positional_options_)
                                                   .allow_unregistered()
                                                   .style(OPTION_STYLE)
                                                   .run();

            po::variables_map global_arguments;
            po::store(global_parsed, global_arguments);
            auto it = global_arguments.find("command");
            if (it != global_arguments.end()) {
                result.command = it->second.as<std::string>();
                global_arguments.erase(it);
            }

            if (!setupCommandOptions(result.command)) {
                auto error = ui_->error();
                // TODO: attempt to continue with default command (empty string)
                //       because "xyz" in "--arg xyz" for command may be interpreted
                //       as command "xyz"
                error << "Invalid command '" << result.command << "'\n";
                // allow printing global help with invalid command
                if (global_arguments.contains("help")) {
                    error << '\n';
                    printGlobalHelp(error);
                }
                return std::nullopt;
            }

            // the meaning of arguments that are both applicable as a global option or a command option
            // is determined by their position (before command is global option, after command is command option);
            // remove all recognized arguments after the command from the set of recognized arguments
            auto is_global_argument_after_command = removeGlobalArgumentsAfterCommand(global_parsed) > 0;
            po::store(global_parsed, result.globalArguments);

            // treat both all unrecognized arguments (this includes all global arguments that came after the command
            // because of the removal from the recognized arguments in the step before) as potential command arguments
            auto global_unrecognized_arguments = po::collect_unrecognized(
                global_parsed.options,
                boost::program_options::collect_unrecognized_mode::include_positional);

            assert(command_options_);
            po::options_description all_command_options;
            all_command_options.add(command_options_->options);
            all_command_options.add(command_options_->hiddenOptions);
            // parse all unrecognized arguments as arguments of the command
            auto command_parsed = po::command_line_parser { global_unrecognized_arguments }
                                      .options(all_command_options)
                                      .positional(command_options_->positionalOptions)
                                      .allow_unregistered()
                                      .style(OPTION_STYLE)
                                      .run();
            po::store(command_parsed, result.commandArguments);

            auto command_unrecognized_arguments = po::collect_unrecognized(command_parsed.options, boost::program_options::collect_unrecognized_mode::exclude_positional);
            // if there was a global argument after the command, parse the options again that were not recognized by the command
            if (is_global_argument_after_command) {
                po::parsed_options second_global_parsed = po::command_line_parser { command_unrecognized_arguments }
                                                              .options(all_global_options)
                                                              .allow_unregistered()
                                                              .style(OPTION_STYLE)
                                                              .run();
                po::store(second_global_parsed, result.globalArguments);

                command_unrecognized_arguments = po::collect_unrecognized(second_global_parsed.options, boost::program_options::collect_unrecognized_mode::exclude_positional);
            }

            {
                auto output = ui_->output();
                bool is_global_help = result.globalArguments.at("help").as<bool>();
                bool is_command_help = result.commandArguments.at("help").as<bool>();
                if (is_global_help) {
                    printGlobalHelp(output);
                    if (is_command_help) {
                        output << '\n';
                    } else {
                        return std::nullopt;
                    }
                }
                if (is_command_help) {
                    printCurrentCommandHelp(output);
                    return std::nullopt;
                }
            }

            // remaining unrecognized arguments must be invalid
            if (!command_unrecognized_arguments.empty()) {
                auto error = ui_->error();
                for (auto&& invalid_argument : command_unrecognized_arguments) {
                    error << "Invalid argument detected: '" << invalid_argument << "'\n";
                }
                error << "\n"
                      << "Use --help as a global option to print all available commands and global options.\n"
                      << "Use --help after a specific command for details about that command.\n";
                return std::nullopt;
            }

            // handle required options and registered notifiers
            po::notify(result.globalArguments);
            po::notify(result.commandArguments);

            return result;
        } catch (const boost::program_options::error& error) {
            ui_->error() << "Invalid command-line usage: " << error.what() << "\n\n"
                         << "Use --help as a global option to print all available commands and global options.\n"
                         << "Use --help after a specific command for details about that command.\n";
            return std::nullopt;
        }
    }

private:
    /**
     * Mark all detected global arguments that were specified after command from the parsing result as unrecognized.
     *
     * @return number of modified arguments
     */
    [[nodiscard]] static std::size_t removeGlobalArgumentsAfterCommand(boost::program_options::parsed_options& parsed)
    {
        std::size_t num_of_global_arguments_after_command { 0 };
        bool encountered_command { false };
        for (auto& option : parsed.options) {
            if (option.position_key != -1) {
                // first positional argument is command; ignore all others
                encountered_command = true;
            } else if (encountered_command) {
                if (!option.unregistered) {
                    ++num_of_global_arguments_after_command;
                }
                option.unregistered = true;
            }
        }
        return num_of_global_arguments_after_command;
    }

    void setupGlobalOptions()
    {
        namespace po = boost::program_options;
        // clang-format off
        global_options_.add_options()
            ("debug", po::bool_switch(), "Turn on debug output")
            ("help", po::bool_switch(), "Print general help");
        hidden_global_options_.add_options()
            ("command", po::value<std::string>(), "Command to execute")
            ("subargs", po::value<std::vector<std::string>>(), "Arguments for command");
        // clang-format on
    }

    void setupPositionalOptions()
    {
        // use first positional argument as command
        positional_options_.add("command", 1);
        // all remaining arguments are arguments of that command
        positional_options_.add("subargs", -1);
    }

    bool setupCommandOptions(const std::string& command)
    {
        namespace po = boost::program_options;

        command_options_ = std::make_unique<CommandOptions>(CommandOptions {
            .options = po::options_description { "Command '" + command + "' options" },
            .hiddenOptions = { },
            .positionalOptions = { } });

        // "ignore" first positional argument because it will be the command again
        command_options_->positionalOptions.add("command", 1);
        command_options_->hiddenOptions.add_options()("command", po::value<std::string>());
        // add help
        // clang-format off
        command_options_->options.add_options()
            ("help", po::bool_switch(), "Print help of command");
        // clang-format on
        return command_registry_.fillCommandLineOptions(command, *command_options_);
    }

    void printGlobalHelp(UiStream& out)
    {
        out << "Usage:\n"
            << "  " << DEFAULT_USAGE << "\n\n"
            << "Commands:\n";
        for (auto&& command : command_registry_.availableCommands()) {
            if (!command.empty()) {
                out << "  " << command << "\n";
            }
        }
        out << "\n"
            << global_options_;
    }

    void printCurrentCommandHelp(UiStream& out)
    {
        assert(command_options_);
        out << "Usage:\n"
            << "  " << command_options_->usage.value_or(std::string { DEFAULT_USAGE }) << "\n\n";
        out << command_options_->options;
    }

private:
    static constexpr std::string_view DEFAULT_USAGE = "qq [global_options] command [command_options]";

    static constexpr auto OPTION_STYLE = boost::program_options::command_line_style::default_style & ~static_cast<std::uint64_t>(boost::program_options::command_line_style::allow_guessing);

private:
    Ui* ui_;
    CommandRegistry command_registry_;
    boost::program_options::options_description global_options_ { "Global options" };
    boost::program_options::options_description hidden_global_options_ { };
    boost::program_options::positional_options_description positional_options_;
    std::unique_ptr<CommandOptions> command_options_;
};

#endif // QQ_COMMAND_LINE_PARSER_H
