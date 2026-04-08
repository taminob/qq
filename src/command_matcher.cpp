#include "command_matcher.h"

std::optional<Command> CommandMatcher::match(std::string_view command_name, const std::vector<std::string>& arguments)
{
    auto it = known_commands_.find(command_name);
    return Command {  };
}
