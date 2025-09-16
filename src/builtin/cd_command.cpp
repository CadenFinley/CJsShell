#include "cd_command.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

#include "error_out.h"
#include "suggestion_utils.h"

#define PRINT_ERROR(MSG)                             \
  do {                                               \
    last_terminal_output_error = (MSG);              \
    std::cerr << last_terminal_output_error << '\n'; \
  } while (0)

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
      PRINT_ERROR("cjsh: HOME environment variable is not set");
      return 1;
    }
    target_dir = home_dir;
  }

  if (target_dir == "-") {
    if (previous_directory.empty()) {
      PRINT_ERROR("cjsh: No previous directory");
      return 1;
    }
    target_dir = previous_directory;
  }

  if (target_dir[0] == '~') {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      PRINT_ERROR(
          "cjsh: Cannot expand '~' - HOME environment variable is not set");
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
      auto suggestions = suggestion_utils::generate_cd_suggestions(target_dir, current_directory);
      ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "cd", target_dir + ": no such file or directory", suggestions};
      print_error(error);
      last_terminal_output_error = "cd: " + target_dir + ": no such file or directory";
      return 1;
    }

    if (!std::filesystem::is_directory(dir_path)) {
      ErrorInfo error = {ErrorType::INVALID_ARGUMENT, "cd", target_dir + ": not a directory", {}};
      print_error(error);
      last_terminal_output_error = "cd: " + target_dir + ": not a directory";
      return 1;
    }

    std::string old_directory = current_directory;

    std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
    current_directory = canonical_path.string();

    if (chdir(current_directory.c_str()) != 0) {
      PRINT_ERROR("cd: " + std::string(strerror(errno)));
      return 1;
    }

    setenv("PWD", current_directory.c_str(), 1);

    previous_directory = old_directory;

    return 0;
  } catch (const std::filesystem::filesystem_error& e) {
    PRINT_ERROR("cd: " + std::string(e.what()));
    return 1;
  } catch (const std::exception& e) {
    PRINT_ERROR("cd: unexpected error: " + std::string(e.what()));
    return 1;
  }
}

int change_directory_with_bookmarks(
    const std::string& dir, std::string& current_directory,
    std::string& previous_directory, std::string& last_terminal_output_error,
    std::unordered_map<std::string, std::string>& directory_bookmarks) {
  std::string target_dir = dir;

  if (target_dir.empty() || target_dir == "~") {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      PRINT_ERROR("cjsh: HOME environment variable is not set");
      return 1;
    }
    target_dir = home_dir;
  }

  if (target_dir == "-") {
    if (previous_directory.empty()) {
      PRINT_ERROR("cjsh: No previous directory");
      return 1;
    }
    target_dir = previous_directory;
  }

  if (target_dir[0] == '~') {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      PRINT_ERROR(
          "cjsh: Cannot expand '~' - HOME environment variable is not set");
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

      if (!std::filesystem::exists(dir_path)) {
        auto bookmark_it = directory_bookmarks.find(target_dir);
        if (bookmark_it != directory_bookmarks.end()) {
          dir_path = bookmark_it->second;
        }
      }
    }

    if (!std::filesystem::exists(dir_path)) {
      auto suggestions = suggestion_utils::generate_cd_suggestions(target_dir, current_directory);
      ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "cd", target_dir + ": no such file or directory", suggestions};
      print_error(error);
      last_terminal_output_error = "cd: " + target_dir + ": no such file or directory";
      return 1;
    }

    if (!std::filesystem::is_directory(dir_path)) {
      ErrorInfo error = {ErrorType::INVALID_ARGUMENT, "cd", target_dir + ": not a directory", {}};
      print_error(error);
      last_terminal_output_error = "cd: " + target_dir + ": not a directory";
      return 1;
    }

    std::string old_directory = current_directory;

    std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
    current_directory = canonical_path.string();

    if (chdir(current_directory.c_str()) != 0) {
      PRINT_ERROR("cd: " + std::string(strerror(errno)));
      return 1;
    }

    setenv("PWD", current_directory.c_str(), 1);

    previous_directory = old_directory;

    update_directory_bookmarks(current_directory, directory_bookmarks);

    return 0;
  } catch (const std::filesystem::filesystem_error& e) {
    PRINT_ERROR("cd: " + std::string(e.what()));
    return 1;
  } catch (const std::exception& e) {
    PRINT_ERROR("cd: unexpected error: " + std::string(e.what()));
    return 1;
  }
}
