#include "cd_command.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

#include "error_out.h"
#include "suggestion_utils.h"
#include "utils/bookmark_database.h"

void update_directory_bookmarks(
    const std::string& dir_path,
    std::unordered_map<std::string, std::string>& directory_bookmarks) {
    std::filesystem::path path(dir_path);
    std::string basename = path.filename().string();
    if (!basename.empty() && basename != "." && basename != "..") {
        directory_bookmarks[basename] = dir_path;
    }
}

int change_directory(const std::string& dir, std::string& current_directory,
                     std::string& previous_directory,
                     std::string& last_terminal_output_error) {
    std::string target_dir = dir;

    if (target_dir.empty() || target_dir == "~") {
        const char* home_dir = getenv("HOME");
        if (!home_dir) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                               "cd",
                               "HOME environment variable is not set",
                               {}};
            print_error(error);
            last_terminal_output_error =
                "cd: HOME environment variable is not set";
            return 1;
        }
        target_dir = home_dir;
    }

    if (target_dir == "-") {
        if (previous_directory.empty()) {
            ErrorInfo error = {
                ErrorType::RUNTIME_ERROR, "cd", "No previous directory", {}};
            print_error(error);
            last_terminal_output_error = "cd: No previous directory";
            return 1;
        }
        target_dir = previous_directory;
    }

    if (target_dir[0] == '~') {
        const char* home_dir = getenv("HOME");
        if (!home_dir) {
            ErrorInfo error = {
                ErrorType::RUNTIME_ERROR,
                "cd",
                "Cannot expand '~' - HOME environment variable is not set",
                {}};
            print_error(error);
            last_terminal_output_error =
                "cd: Cannot expand '~' - HOME environment variable is not set";
            return 1;
        }
        target_dir.replace(0, 1, home_dir);
    }

    std::filesystem::path dir_path;

    try {
        if (std::filesystem::path(target_dir).is_absolute()) {
            dir_path = target_dir;
        } else {
            dir_path = std::filesystem::path(current_directory) / target_dir;
        }

        if (!std::filesystem::exists(dir_path)) {
            auto suggestions = suggestion_utils::generate_cd_suggestions(
                target_dir, current_directory);
            ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "cd",
                               target_dir + ": no such file or directory",
                               suggestions};
            print_error(error);
            last_terminal_output_error =
                "cd: " + target_dir + ": no such file or directory";
            return 1;
        }

        if (!std::filesystem::is_directory(dir_path)) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "cd",
                               target_dir + ": not a directory",
                               {}};
            print_error(error);
            last_terminal_output_error =
                "cd: " + target_dir + ": not a directory";
            return 1;
        }

        std::string old_directory = current_directory;

        std::filesystem::path canonical_path =
            std::filesystem::canonical(dir_path);
        current_directory = canonical_path.string();

        if (chdir(current_directory.c_str()) != 0) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                               "cd",
                               std::string(strerror(errno)),
                               {}};
            print_error(error);
            last_terminal_output_error = "cd: " + std::string(strerror(errno));
            return 1;
        }

        setenv("PWD", current_directory.c_str(), 1);

        previous_directory = old_directory;

        return 0;
    } catch (const std::filesystem::filesystem_error& e) {
        ErrorInfo error = {
            ErrorType::RUNTIME_ERROR, "cd", std::string(e.what()), {}};
        print_error(error);
        last_terminal_output_error = "cd: " + std::string(e.what());
        return 1;
    } catch (const std::exception& e) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                           "cd",
                           "unexpected error: " + std::string(e.what()),
                           {}};
        print_error(error);
        last_terminal_output_error =
            "cd: unexpected error: " + std::string(e.what());
        return 1;
    }
}

int change_directory_smart(const std::string& dir,
                           std::string& current_directory,
                           std::string& previous_directory,
                           std::string& last_terminal_output_error) {
    std::string target_dir = dir;

    if (target_dir.empty() || target_dir == "~") {
        const char* home_dir = getenv("HOME");
        if (!home_dir) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                               "cd",
                               "HOME environment variable is not set",
                               {}};
            print_error(error);
            last_terminal_output_error =
                "cd: HOME environment variable is not set";
            return 1;
        }
        target_dir = home_dir;
    }

    if (target_dir == "-") {
        if (previous_directory.empty()) {
            ErrorInfo error = {
                ErrorType::RUNTIME_ERROR, "cd", "No previous directory", {}};
            print_error(error);
            last_terminal_output_error = "cd: No previous directory";
            return 1;
        }
        target_dir = previous_directory;
    }

    if (target_dir[0] == '~') {
        const char* home_dir = getenv("HOME");
        if (!home_dir) {
            ErrorInfo error = {
                ErrorType::RUNTIME_ERROR,
                "cd",
                "Cannot expand '~' - HOME environment variable is not set",
                {}};
            print_error(error);
            last_terminal_output_error =
                "cd: Cannot expand '~' - HOME environment variable is not set";
            return 1;
        }
        target_dir.replace(0, 1, home_dir);
    }

    std::filesystem::path dir_path;
    bool used_bookmark = false;

    try {
        if (std::filesystem::path(target_dir).is_absolute()) {
            dir_path = target_dir;
        } else {
            dir_path = std::filesystem::path(current_directory) / target_dir;

            // If the relative path doesn't exist, check bookmarks
            if (!std::filesystem::exists(dir_path)) {
                auto bookmark_path =
                    bookmark_database::g_bookmark_db.get_bookmark(target_dir);
                if (bookmark_path.has_value()) {
                    dir_path = bookmark_path.value();
                    used_bookmark = true;
                }
            }
        }

        if (!std::filesystem::exists(dir_path)) {
            auto suggestions = suggestion_utils::generate_cd_suggestions(
                target_dir, current_directory);
            ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "cd",
                               target_dir + ": no such file or directory",
                               suggestions};
            print_error(error);
            last_terminal_output_error =
                "cd: " + target_dir + ": no such file or directory";
            return 1;
        }

        if (!std::filesystem::is_directory(dir_path)) {
            ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                               "cd",
                               target_dir + ": not a directory",
                               {}};
            print_error(error);
            last_terminal_output_error =
                "cd: " + target_dir + ": not a directory";
            return 1;
        }

        std::string old_directory = current_directory;

        std::filesystem::path canonical_path =
            std::filesystem::canonical(dir_path);
        current_directory = canonical_path.string();

        if (chdir(current_directory.c_str()) != 0) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                               "cd",
                               std::string(strerror(errno)),
                               {}};
            print_error(error);
            last_terminal_output_error = "cd: " + std::string(strerror(errno));
            return 1;
        }

        setenv("PWD", current_directory.c_str(), 1);

        previous_directory = old_directory;

        // Update bookmark database with the new directory, but only if we
        // didn't use a bookmark to get here and only if we used a relative path
        // (to avoid overriding bookmarks when navigating via relative paths)
        if (!used_bookmark &&
            !std::filesystem::path(target_dir).is_absolute()) {
            std::filesystem::path path(current_directory);
            std::string basename = path.filename().string();
            if (!basename.empty() && basename != "." && basename != "..") {
                auto add_result = bookmark_database::g_bookmark_db.add_bookmark(
                    basename, current_directory);
                if (add_result.is_error()) {
                    // Don't fail the cd operation, just log the warning
                    print_error(
                        {ErrorType::RUNTIME_ERROR,
                         "cd",
                         "Failed to update bookmark: " + add_result.error(),
                         {}});
                }
            }
        }

        return 0;
    } catch (const std::filesystem::filesystem_error& e) {
        ErrorInfo error = {
            ErrorType::RUNTIME_ERROR, "cd", std::string(e.what()), {}};
        print_error(error);
        last_terminal_output_error = "cd: " + std::string(e.what());
        return 1;
    } catch (const std::exception& e) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                           "cd",
                           "unexpected error: " + std::string(e.what()),
                           {}};
        print_error(error);
        last_terminal_output_error =
            "cd: unexpected error: " + std::string(e.what());
        return 1;
    }
}
