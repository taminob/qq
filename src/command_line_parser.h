#ifndef QQ_COMMAND_LINE_PARSER_H
#define QQ_COMMAND_LINE_PARSER_H

#include <iostream>
#include <span>

#include <boost/program_options.hpp>

// Commands
//   - run (default?)
//   - save
//   - edit
//   - delete
//   - list
//   - search
//   - register (?, allow definition of new commands)
//   - scan (?, check config directory for changes)
//   - update-autocomplete (for shell tab-completion)
//
// Command Options:
//   - --name
//   - --tags
//   - --autotag
//   - --autorun (automatically run closest match)
//   - --noconfirm
//   - --class / --category (?)
//   - --command (positional?)

class CommandLineParser {
public:
    struct CommandLineArguments {
        boost::program_options::variables_map globalArguments;
        std::string command;
        boost::program_options::variables_map commandArguments;
    };

public:
    CommandLineParser()
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
        CommandLineArguments result;

        po::parsed_options global_parsed = po::command_line_parser { static_cast<int>(args.size()), args.data() }
                                               .options(global_options_)
                                               .positional(positional_options_)
                                               .allow_unregistered()
                                               .run();

        po::variables_map global_arguments;
        po::store(global_parsed, global_arguments);
        auto it = global_arguments.find("command");
        if (it != global_arguments.end()) {
            result.command = it->second.as<std::string>();
            global_arguments.erase(it);
        }
        result.globalArguments = std::move(global_arguments);

        setupCommandOptions(result.command);

        // the meaning of arguments that are both applicable as a global option or a command option
        // is determined by their position (before command is global option, after command is command option);
        // remove all recognized arguments after the command from the set of recognized arguments
        auto is_global_argument_after_command = removeGlobalArgumentsAfterCommand(global_parsed) > 0;

        // treat both all unrecognized arguments (this includes all global arguments that came after the command
        // because of the removal from the recognized arguments in the step before) as potential command arguments
        auto global_unrecognized_arguments = po::collect_unrecognized(
            global_parsed.options,
            boost::program_options::collect_unrecognized_mode::include_positional);

        // parse all unrecognized arguments as arguments of the command
        auto command_parsed = po::command_line_parser { global_unrecognized_arguments }
                                  .options(command_description_)
                                  .positional(boost::program_options::positional_options_description { })
                                  .allow_unregistered()
                                  .run();
        po::store(command_parsed, result.commandArguments);

        auto command_unrecognized_arguments = po::collect_unrecognized(command_parsed.options, boost::program_options::collect_unrecognized_mode::exclude_positional);
        // if there was a global argument after the command, parse the options again that were not recognized by the command
        if (is_global_argument_after_command) {
            po::parsed_options second_global_parsed = po::command_line_parser { command_unrecognized_arguments }
                                                          .options(global_options_)
                                                          .allow_unregistered()
                                                          .run();
            po::store(second_global_parsed, result.globalArguments);

            command_unrecognized_arguments = po::collect_unrecognized(global_parsed.options, boost::program_options::collect_unrecognized_mode::exclude_positional);
        }

        // handle notifiers before invalid arguments to allow printing help regardless of invalid arguments
        po::notify(result.globalArguments);
        po::notify(result.commandArguments);
        if (exit_program_) {
            return std::nullopt;
        }

        // remaining unrecognized arguments must be invalid
        if (!command_unrecognized_arguments.empty()) {
            for (auto&& invalid_argument : command_unrecognized_arguments) {
                std::cerr << "Invalid argument detected: '" << invalid_argument << "'\n";
            }
            std::cerr << "\n"
                      << "Use --help as a global option to print all available commands and global options.\n"
                      << "Use --help after a specific command for details about that command.\n";
            return std::nullopt;
        }

        return result;
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
                if (option.unregistered) {
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
        ("command", po::value<std::string>(), "command to execute")
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
        auto commands = getCommands();
        auto it = commands.find(command);
        if (it == commands.end()) {
            return false;
        }
        (this->*it->second)();
        return true;
    }

    void setupRunCommand()
    {
        namespace po = boost::program_options;
        // clang-format off
        command_description_.add_options()
            ("help", po::bool_switch()->notifier([this](bool show_help) {
                    if (show_help) {
                        printCurrentCommandHelp(std::cout);
                        exit_program_ = true;
                    }
                }),
            "Print help of command");
        // clang-format on
    }

    void setupStoreCommand()
    {
    }

    static std::unordered_map<std::string, void (CommandLineParser::*)()> getCommands()
    {
        return {
            { "run", &CommandLineParser::setupRunCommand },
        };
    }

    void printGlobalHelp(std::ostream& out)
    {
        out << global_options_;
    }

    void printCurrentCommandHelp(std::ostream& out)
    {
        out << command_description_;
    }

private:
    boost::program_options::options_description global_options_ { "Global options" };
    boost::program_options::positional_options_description positional_options_;
    boost::program_options::options_description command_description_ { "Command options" };
    boost::program_options::options_description command_positional_options_;
    bool exit_program_ { false };
};

#endif // QQ_COMMAND_LINE_PARSER_H
