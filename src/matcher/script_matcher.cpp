#include "script_matcher.h"
#include "plugin_type.h"
#include "utils/filesystem_utils.h"

#include <string>

std::vector<std::filesystem::path> SimpleScriptMatcher::match(const std::vector<std::filesystem::path>& paths, std::string_view search_pattern)
{
    std::vector<std::filesystem::path> result;
    iterateFiles(paths, [&](auto&& path) {
        if (isFileMatch(path, search_pattern)) {
            result.push_back(path);
        }
    });
    return result;
}

bool SimpleScriptMatcher::isFileMatch(const std::filesystem::path& file_path, std::string_view search_pattern)
{
    std::size_t num_matches { };
    std::string current_line;
    std::ifstream file { file_path }; // TODO? open shared libraries as binary
    while (std::getline(file, current_line)) {
        if (current_line.contains(search_pattern)) {
            ++num_matches;
            // TODO? break on first match?
        }
    }
    return num_matches > 0;
}

std::vector<std::filesystem::path> FunctionNameScriptMatcher::match(const std::vector<std::filesystem::path>& paths, std::string_view search_pattern) const
{
    std::vector<std::filesystem::path> result;
    iterateFiles(paths, [&](auto&& path) {
        if (isFileMatch(path, search_pattern)) {
            result.push_back(path);
        }
    });
    return result;
}

bool FunctionNameScriptMatcher::isFileMatch(const std::filesystem::path& file_path, std::string_view search_pattern) const
{
    std::size_t num_matches { };
    std::string current_line;
    std::ifstream file { file_path }; // TODO? open shared libraries as binary
    while (std::getline(file, current_line)) {
        if (matchScore(current_line, search_pattern, detectPluginType(*config_, file_path))) {
            ++num_matches;
            // TODO? break on first match?
        }
    }
    return num_matches > 0;
}

bool FunctionNameScriptMatcher::matchScore(std::string_view matched_line, std::string_view search_pattern, PluginType type)
{
    auto match_pos = matched_line.find(search_pattern);
    if (match_pos == std::string::npos) {
        return false;
    }
    auto match_end_pos = match_pos + search_pattern.size();
    if (std::isalnum(matched_line.at(match_end_pos)) != 0 || (match_pos > 0 && (std::isalnum(matched_line.at(match_pos - 1)) != 0))) {
        // match is embedded in another identifier
        return false;
    }

    auto check_function_keyword = [](std::string_view word, std::string_view keyword) {
        auto pos = word.find(keyword);
        if (pos == std::string::npos) {
            // keyword not found in word
            return false;
        }

        // check that there is a whitespace character after keyword
        auto pos_after_keyword = pos + keyword.size();
        if (pos_after_keyword >= word.size()) {
            return false;
        }
        return std::isspace(word[pos_after_keyword]) != 0;
    };

    auto check_function_parentheses = [](std::string_view word) {
        auto parenthesis_open_pos = word.find('(');
        if (parenthesis_open_pos == std::string::npos) {
            return false;
        }
        auto parenthesis_close_pos = word.find_last_of(')', parenthesis_open_pos);
        if (parenthesis_close_pos == std::string::npos) {
            return false;
        }

        // check for only whitespaces before opening parenthesis
        return std::ranges::all_of(word.substr(0, parenthesis_open_pos), [](auto&& character) {
            return std::isspace(character) != 0;
        });
    };

    switch (type) {
    case PluginType::lua:
        return check_function_keyword(matched_line.substr(0, match_pos), "function")
            && check_function_parentheses(matched_line.substr(match_pos, match_end_pos));
    case PluginType::python:
        return check_function_keyword(matched_line.substr(0, match_pos), "def")
            && check_function_parentheses(matched_line.substr(match_pos, match_end_pos));
    case PluginType::shell:
        break;
    case PluginType::c:
        [[fallthrough]];
    case PluginType::cpp:
        [[fallthrough]];
    case PluginType::unknown:
        break;
    }
    return true;
}
