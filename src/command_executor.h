#ifndef QQ_COMMAND_EXECUTOR_H
#define QQ_COMMAND_EXECUTOR_H

#include <ppplugin/plugin.h>

#include <utility>

struct CommandResult {
    std::optional<std::string> error;
};

class CommandExecutor {
public:
    explicit CommandExecutor(ppplugin::Plugin plugin, std::string function_name)
        : plugin_ { std::move(plugin) }
        , function_name_ { std::move(function_name) }
    {
    }

    template <typename... Args>
    CommandResult execute(Args&&... arguments)
    {
        CommandResult result;

        try {
            auto call_result = plugin_.call<void, Args...>(
                function_name_, std::forward<Args>(arguments)...);

            if (!call_result) {
                result.error = call_result.error()->what();
            }
        } catch (const std::exception& exception) {
            result.error = exception.what();
        } catch (...) {
            result.error = "<unknown exception>";
        }

        return result;
    }

private:
    ppplugin::Plugin plugin_;
    std::string function_name_;
};

#endif // QQ_COMMAND_EXECUTOR_H
