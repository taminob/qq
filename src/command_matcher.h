#ifndef QQ_COMMAND_MATCHER_H
#define QQ_COMMAND_MATCHER_H

#include "command.h"
#include "utils.h"

#include <string>
#include <string_view>
#include <vector>

class CommandMatcher {
public:
    CommandMatcher() = default;

    std::optional<Command> match(std::string_view command_name, const std::vector<std::string>& arguments);

private:
    std::unordered_map<std::string, std::string, StringHash, std::equal_to<>> known_commands_;
};

#endif // QQ_COMMAND_MATCHER_H
