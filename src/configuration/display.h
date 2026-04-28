#ifndef QQ_CONFIGURATION_DISPLAY_H
#define QQ_CONFIGURATION_DISPLAY_H

#include "configuration_option.h"
#include "ui.h"
#include "utils/filesystem_utils.h"

#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include <ppplugin/plugin.h>

class DisplayConfiguration {
public:
    DisplayConfiguration() = default;

    void read(ppplugin::Plugin& plugin)
    {
        if (auto new_value = plugin.global<int>("SCREEN_WIDTH")) {
            screen_width_.overwrite(ConfigurationLayer::file, new_value.value());
        }
        if (auto new_value = plugin.global<int>("CANDIDATE_LIMIT")) {
            candidate_display_limit_.overwrite(ConfigurationLayer::file, new_value.value());
        }
    }

    void normalize()
    {
    }

    /**
     *
     */
    [[nodiscard]] bool verify(Ui& ui) const
    {
        constexpr std::size_t MAX_SCREEN_WIDTH = 1000;
        if (screen_width_.get() > MAX_SCREEN_WIDTH) {
            // warn user, but proceed anyway
            ui.warn() << "Configured screen width (" << screen_width_.get() << ") exceeds maximum value (" << MAX_SCREEN_WIDTH << ")\n";
        }
        return true;
    }

    [[nodiscard]] std::size_t screenWidth() const
    {
        return screen_width_.get();
    }

    [[nodiscard]] std::size_t candidateDisplayLimit() const
    {
        return candidate_display_limit_.get();
    }

private:
    [[nodiscard]] static constexpr std::size_t defaultScreenWidth() { return 40; }
    [[nodiscard]] static constexpr std::size_t defaultCandidateDisplayLimit() { return 10; }

private:
    ConfigurationOption<std::size_t> screen_width_ { defaultScreenWidth() };
    ConfigurationOption<std::size_t> candidate_display_limit_ { defaultCandidateDisplayLimit() };
};

#endif // QQ_CONFIGURATION_DISPLAY_H
