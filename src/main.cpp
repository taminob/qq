#include "command_line_parser.h"
#include "commands/command_registry.h"
#include "commands/run_command.h"
#include "commands/store_command.h"
#include "configuration/configuration.h"
#include "ui.h"

#include <span>

#include <ppplugin/plugin.h>

int main(int argc, char* argv[])
{
    Ui ui;

    try {
        Configuration config { &ui };

        CommandRegistry commands { &ui, &config };
        commands.registerCommand<RunCommand>();
        commands.registerCommand<StoreCommand>();

        CommandLineParser cli_parser { &ui, commands };
        auto cli_arguments = cli_parser.parse(std::span { argv, static_cast<std::size_t>(argc) });
        if (!cli_arguments.has_value()) {
            // error output is part of command-line parser
            return 2;
        }
        auto&& arguments = cli_arguments.value();

        // setup UI
        ui.enableDebug(arguments.globalArguments.at("debug").as<bool>());

        // load config after arguments are parsed and UI is set up
        config.reload();

        if (!commands.run(arguments.command, arguments.commandArguments)) {
            return 3;
        }
    } catch (const std::exception& exception) {
        ui.error() << "Fatal error: '" << exception.what() << "'\n";
        return 1;
    } catch (...) {
        ui.error() << "Unknown fatal error!\n";
        return 1;
    }
    return 0;
}
