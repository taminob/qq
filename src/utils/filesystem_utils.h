#ifndef QQ_UTILS_FILESYSTEM_UTILS_H
#define QQ_UTILS_FILESYSTEM_UTILS_H

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

constexpr std::string_view applicationName()
{
    return "qq";
}

/**
 * Get directory in which the application may store its data.
 *
 * @note this function is not thread-safe
 */
inline std::filesystem::path defaultAppDataDirectory()
{
    auto get_data_directory = []() -> std::filesystem::path {
        // NOLINTNEXTLINE(concurrency-mt-unsafe); behavior documented for function
        if (const char* data_home = std::getenv("XDG_DATA_HOME")) {
            return data_home;
        }
        // NOLINTNEXTLINE(concurrency-mt-unsafe); behavior documented for function
        if (const char* home = std::getenv("HOME")) {
            return std::string { home } + "/.local/share";
        }

        // fall-back; will likely fail due to permission issues
        return "/var/lib/";
    };

    return get_data_directory() / applicationName();
}

/**
 * Get directory in which the application may store its configuration files.
 *
 * @note this function is not thread-safe
 */
inline std::filesystem::path defaultAppConfigDirectory()
{
    auto get_config_directory = []() -> std::filesystem::path {
        // NOLINTNEXTLINE(concurrency-mt-unsafe); behavior documented for function
        if (const char* data_home = std::getenv("XDG_CONFIG_HOME")) {
            return data_home;
        }
        // NOLINTNEXTLINE(concurrency-mt-unsafe); behavior documented for function
        if (const char* home = std::getenv("HOME")) {
            return std::string { home } + "/.config";
        }

        // fall-back; will likely fail due to permission issues
        return "/etc";
    };

    return get_config_directory() / applicationName();
}

/**
 * Get directory for temporary files of the application.
 */
inline std::filesystem::path temporaryAppDirectory(bool create_if_necessary = false)
{
    auto&& path = std::filesystem::temp_directory_path() / applicationName();
    if (create_if_necessary) {
        std::filesystem::create_directories(path);
    }
    return path;
}

/**
 * If given file path already exists, return next available file path.
 * For this, the next available number is added as an extension before all existing extensions.
 * E.g. "/path/x.y.z" -> "/path/x.1.y.z" if "/path/x.y.z" already exists
 */
inline std::filesystem::path findNextAvailableFilePath(std::filesystem::path path)
{
    if (!std::filesystem::exists(path)) {
        // no duplicate, no change needed
        return path;
    }
    std::string all_extensions;
    while (path.has_extension()) {
        all_extensions += path.extension();
        path.replace_extension();
    }
    int counter { 1 };
    auto path_without_extensions = path;
    while (std::filesystem::exists(path.replace_extension("." + std::to_string(counter) + all_extensions))) {
        ++counter;
        path = path_without_extensions;
    }
    return path;
}

/**
 * Get path to a unique temporary file.
 */
inline std::filesystem::path temporaryFile(std::string_view base_name = "temp")
{
    auto&& directory = temporaryAppDirectory(true);
    return findNextAvailableFilePath(directory / base_name);
}

template <typename Container, typename Func>
    requires std::invocable<Func, std::filesystem::path>
    && std::ranges::range<Container>
    && std::convertible_to<std::ranges::range_value_t<Container>, std::filesystem::path>
auto iterateFiles(Container&& paths, Func&& func)
{
    for (auto&& path : paths) {
        if (!std::filesystem::exists(path)) {
            // skip non-existing paths
            continue;
        }
        if (std::filesystem::is_directory(path)) {
            // recursively iterate directories
            for (auto&& file_path_it : std::filesystem::recursive_directory_iterator { path }) {
                if (file_path_it.is_directory()) {
                    // do not call callback for directories
                    continue;
                }
                auto&& file_path = file_path_it.path();
                func(file_path);
            }
        } else {
            func(path);
        }
    }
}

[[nodiscard]] inline bool isFileInDirectory(const std::filesystem::path& file, const std::filesystem::path& directory)
{
    auto&& canonical_dir = std::filesystem::weakly_canonical(directory);
    auto&& canonical_file = std::filesystem::weakly_canonical(file);

    auto [it_dir, it_file] = std::ranges::mismatch(canonical_dir, canonical_file);
    return it_dir == canonical_dir.end();
}

template <typename Stream>
inline bool printFile(Stream&& out_stream, const std::filesystem::path& file_path)
{
    std::ifstream file { file_path };
    if (!file) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        out_stream << line << '\n';
    }
    return true;
}

#endif // QQ_UTILS_FILESYSTEM_UTILS_H
