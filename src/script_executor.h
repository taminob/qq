#ifndef QQ_SCRIPT_EXECUTOR_H
#define QQ_SCRIPT_EXECUTOR_H

#include "plugin_type.h"
#include "script.h"
#include "ui.h"

#include <ppplugin/plugin.h>

#include <utility>

struct ScriptResult {
    std::optional<std::string> error;
};

class ScriptExecutor {
public:
    explicit ScriptExecutor(Ui* ui, Configuration* config)
        : ui_ { ui }
        , config_ { config }
    {
        assert(ui_ != nullptr);
        assert(config_ != nullptr);
    }

    bool load(const Script& script)
    {
        auto plugin = loadScript(script);
        if (!plugin) {
            ui_->error() << "Unable to load '" << script.pluginType() << "' script " << script.pluginPath() << ": " << plugin.error() << '\n';
            return false;
        }
        plugin_ = std::make_unique<decltype(plugin_)::element_type>(std::move(plugin.value()));
        script_ = std::make_unique<Script>(script);
        return true;
    }

    [[nodiscard]] explicit operator bool()
    {
        return plugin_ && script_;
    }

    template <typename... Args>
    ScriptResult execute(Args&&... arguments)
    {
        ScriptResult result;
        if (!*this) {
            result.error = "no script loaded";
            return result;
        }

        try {
            auto call_result = plugin_->call<std::string, Args...>(
                script_->functionName(), std::forward<Args>(arguments)...);

            if (!call_result) {
                std::ostringstream stream;
                stream << call_result.error();
                result.error = stream.str();
            } else if (!call_result->empty()) {
                ui_->output() << '\n'
                              << call_result.value();
            }
        } catch (const std::exception& exception) {
            result.error = exception.what();
        } catch (...) {
            result.error = "<unknown exception>";
        }

        return result;
    }

private:
    /**
     * This shell plugin directly executes function without
     * keeping the shell open. This allows inheriting the
     * stdout/stderr/stdin file descriptors of this process
     * which enables interactive execution of the command.
     */
    struct CustomShellPlugin {
        static ppplugin::Expected<CustomShellPlugin, ppplugin::LoadError> load(std::filesystem::path script_path)
        {
            if (!std::filesystem::exists(script_path)) {
                return ppplugin::LoadError { ppplugin::LoadErrorCode::fileNotFound };
            }
            return CustomShellPlugin { std::move(script_path) };
        }

        explicit operator bool() const { return std::filesystem::exists(script_path_); }

        template <typename ReturnType, typename... Args>
        ppplugin::CallResult<ReturnType> call(std::string_view function, Args&&... /*arguments*/)
        {
            boost::asio::io_context context;
            boost::process::v2::process process { context, shell_executable_,
                std::array<std::string, 2> { "-c", ". " + ppplugin::convertToShellString(script_path_.string()) + " && " + ppplugin::convertToShellString(function) } };
            process.wait();
            return { };
        }

        template <typename T>
        ppplugin::CallResult<T> global(std::string_view /*variable_name*/) { }
        template <typename T>
        ppplugin::CallResult<void> global(std::string_view /*variable_name*/, T&& /*new_value*/) { }

        /**
         * Must be called before executing a function on this plugin.
         */
        void setShellExecutable(std::string shell_executable)
        {
            shell_executable_ = std::move(shell_executable);
        }

    private:
        explicit CustomShellPlugin(std::filesystem::path script_path)
            : script_path_ { std::move(script_path) }
        {
        }

    private:
        std::filesystem::path script_path_;
        std::string shell_executable_;
    };
    using CustomPlugin = ppplugin::GenericPlugin<ppplugin::CPlugin, ppplugin::CppPlugin, ppplugin::LuaPlugin, ppplugin::PythonPlugin, CustomShellPlugin>;

    [[nodiscard]] ppplugin::Expected<CustomPlugin, ppplugin::LoadError> loadScript(const Script& script, PluginType type_overwrite = PluginType::unknown)
    {
        auto type = (type_overwrite == PluginType::unknown) ? script.pluginType() : type_overwrite;
        switch (type) {
        case PluginType::shell:
            return CustomShellPlugin::load(script.pluginPath()).andThen([this](auto&& plugin) {
                ui_->debug() << "Using shell executable: " << config_->getShellExecutablePath() << '\n';
                plugin.setShellExecutable(config_->getShellExecutablePath());
                return plugin;
            });
        case PluginType::python:
            return ppplugin::PythonPlugin::load(script.pluginPath());
        case PluginType::lua:
            return ppplugin::LuaPlugin::load(script.pluginPath());
        case PluginType::c:
            return ppplugin::CPlugin::load(script.pluginPath());
        case PluginType::cpp:
            return ppplugin::CppPlugin::load(script.pluginPath());
        case PluginType::unknown:
            auto types = std::array { PluginType::shell, PluginType::python, PluginType::lua, PluginType::c, PluginType::cpp };
            auto choice = ui_->userChoice(types);
            return loadScript(script, types.at(choice));
        }
    }

private:
    Ui* ui_;
    Configuration* config_;

    std::unique_ptr<CustomPlugin> plugin_;
    std::unique_ptr<Script> script_;
};

#endif // QQ_SCRIPT_EXECUTOR_H
