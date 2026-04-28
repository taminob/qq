#ifndef QQ_COMMANDS_COMMAND_REGISTRY_H
#define QQ_COMMANDS_COMMAND_REGISTRY_H

#include "configuration/configuration.h"
#include "ui.h"
#include "utils/string_utils.h"

#include <functional>
#include <string>
#include <string_view>

#include <boost/program_options.hpp>

struct CommandOptions {
    boost::program_options::options_description options;
    boost::program_options::options_description hiddenOptions;
    boost::program_options::positional_options_description positionalOptions;

    // string that will be printed in help output of command;
    // e.g. "qq [global_options] some_command [command_options] [names...]"
    std::optional<std::string> usage;
};

template <typename T>
concept IsCommand = requires(T instance) {
    { T::id() } -> std::constructible_from<std::string>;
    { T::fillCommandLineOptions(std::declval<CommandOptions&>()) };

    requires std::constructible_from<T, Ui*, Configuration*, boost::program_options::variables_map>;
    { instance.run() } -> std::same_as<bool>;
};

class CommandRegistry {
public:
    explicit CommandRegistry(Ui* ui, Configuration* config)
        : ui_ { ui }
        , config_ { config }
    {
        assert(ui_ != nullptr);
        assert(config_ != nullptr);
    }

    ~CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = default;
    CommandRegistry(CommandRegistry&&) = default;
    CommandRegistry& operator=(const CommandRegistry&) = default;
    CommandRegistry& operator=(CommandRegistry&&) = default;

    template <typename CommandType>
        requires IsCommand<CommandType>
    void registerCommand(std::optional<std::string_view> override_command_id = std::nullopt)
    {
        RegisteredCommand registered_command {
            .runner = [this](const boost::program_options::variables_map& arguments) -> bool {
                CommandType command { ui_, config_, arguments };
                return command.run();
            },
            .commandLineOptions = [](CommandOptions& command_options) -> void {
                CommandType::fillCommandLineOptions(command_options);
            },
        };
        const std::string command_name { override_command_id.value_or(CommandType::id()) };
        command_runners_.try_emplace(command_name, std::move(registered_command));
    }

    std::optional<bool> run(std::string_view command_name, const boost::program_options::variables_map& arguments)
    {
        auto command = findRegisteredCommand(command_name);
        return command.and_then([&](auto&& value) -> std::optional<bool> { return value.get().runner(arguments); });
    }

    bool fillCommandLineOptions(std::string_view command_name, CommandOptions& command_options) const
    {
        if (auto command = findRegisteredCommand(command_name)) {
            command->get().commandLineOptions(command_options);
            return true;
        }
        return false;
    }

    std::vector<std::string> availableCommands() const
    {
        std::vector<std::string> result;
        result.reserve(command_runners_.size());
        for (auto&& [key, item] : command_runners_) {
            result.push_back(key);
        }
        return result;
    }

private:
    struct RegisteredCommand {
        std::function<bool(const boost::program_options::variables_map&)> runner;
        std::function<void(CommandOptions&)> commandLineOptions;
    };

    [[nodiscard]] std::optional<std::reference_wrapper<RegisteredCommand>> findRegisteredCommand(std::string_view command_name)
    {
        auto it = command_runners_.find(command_name);
        if (it == command_runners_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    [[nodiscard]] std::optional<std::reference_wrapper<const RegisteredCommand>> findRegisteredCommand(std::string_view command_name) const
    {
        auto it = command_runners_.find(command_name);
        if (it == command_runners_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    std::unordered_map<std::string, RegisteredCommand, StringHash, std::equal_to<>> command_runners_;

    Ui* ui_;
    Configuration* config_;
};

#endif // QQ_COMMANDS_COMMAND_REGISTRY_H
