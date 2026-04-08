#include "command_line_parser.h"

#include <span>

#include <ppplugin/plugin.h>

int main(int argc, char* argv[])
{
    CommandLineParser cli_parser;
    auto cli_arguments = cli_parser.parse(std::span { argv, static_cast<std::size_t>(argc) });

    
}
