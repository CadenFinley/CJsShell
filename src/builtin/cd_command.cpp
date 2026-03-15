/*
  cd_command.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "cd_command.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <optional>

#include "builtin_help.h"
#include "error_out.h"
#include "shell.h"
#include "shell_env.h"
#include "suggestion_utils.h"

int cd_command(const std::vector<std::string>& args, std::string& current_directory,
               std::string& previous_directory, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: cd [DIR]", "Change the current directory.",
                                   "Use '-' to switch to the previous directory."})) {
        return 0;
    }

    if (args.size() > 2) {
        ErrorInfo error = {
            ErrorType::INVALID_ARGUMENT, "cd", "too many arguments", {"Usage: cd [directory]"}};
        print_error(error);
        return 2;
    }

    return change_directory(args.size() > 1 ? args[1] : "", current_directory, previous_directory,
                            shell);
}

namespace {

std::optional<std::filesystem::path> resolve_smart_cd_target(const std::string& target_dir,
                                                             const std::string& current_dir) {
    if (target_dir.empty()) {
        return std::nullopt;
    }

    std::filesystem::path current_path(current_dir);
    std::filesystem::path target_path(target_dir);
    std::filesystem::path base_path = current_path;
    std::string lookup_fragment = target_dir;

    std::error_code ec;

    if (target_path.is_absolute()) {
        base_path = target_path.parent_path();
        lookup_fragment = target_path.filename().string();
        if (lookup_fragment.empty()) {
            lookup_fragment = target_path.string();
        }
    } else {
        std::filesystem::path resolved = current_path / target_path;
        std::filesystem::path parent = resolved.parent_path();
        if (parent.empty()) {
            parent = current_path;
        }

        if (std::filesystem::exists(parent, ec)) {
            base_path = parent;
            lookup_fragment = resolved.filename().string();
            if (lookup_fragment.empty()) {
                lookup_fragment = target_path.filename().string();
                if (lookup_fragment.empty()) {
                    lookup_fragment = target_dir;
                }
            }
        } else {
            ec.clear();
        }
    }

    if (lookup_fragment.empty()) {
        lookup_fragment = target_dir;
    }

    std::string base_dir = base_path.empty() ? current_dir : base_path.string();

    auto raw_similar = suggestion_utils::find_similar_entries(lookup_fragment, base_dir, 2);
    if (raw_similar.size() != 1) {
        return std::nullopt;
    }

    std::filesystem::path search_base =
        base_path.empty() ? std::filesystem::path(base_dir) : base_path;
    std::filesystem::path candidate_path = search_base / raw_similar.front();

    if (!std::filesystem::exists(candidate_path, ec) ||
        !std::filesystem::is_directory(candidate_path, ec)) {
        ec.clear();
        return std::nullopt;
    }

    return candidate_path;
}

}  // namespace

int change_directory(const std::string& dir, std::string& current_directory,
                     std::string& previous_directory, Shell* shell) {
    std::string target_dir = dir;

    if (target_dir.empty() || target_dir == "~") {
        if (!cjsh_env::shell_variable_is_set("HOME")) {
            ErrorInfo error = {
                ErrorType::RUNTIME_ERROR, "cd", "HOME environment variable is not set", {}};
            print_error(error);
            return 1;
        }
        target_dir = cjsh_env::get_shell_variable_value("HOME");
    }

    if (target_dir == "-") {
        if (previous_directory.empty()) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR, "cd", "No previous directory", {}};
            print_error(error);
            return 1;
        }
        target_dir = previous_directory;
    }

    if (target_dir[0] == '~') {
        if (!cjsh_env::shell_variable_is_set("HOME")) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                               "cd",
                               "Cannot expand '~' - HOME environment variable is not set",
                               {}};
            print_error(error);
            return 1;
        }
        target_dir.replace(0, 1, cjsh_env::get_shell_variable_value("HOME"));
    }

    std::filesystem::path dir_path;

    try {
        if (std::filesystem::path(target_dir).is_absolute()) {
            dir_path = target_dir;
        } else {
            dir_path = std::filesystem::path(current_directory) / target_dir;
        }

        if (!std::filesystem::exists(dir_path)) {
            if (config::smart_cd_enabled && !config::minimal_mode && !config::secure_mode) {
                auto smart_target = resolve_smart_cd_target(target_dir, current_directory);
                if (smart_target.has_value()) {
                    dir_path = *smart_target;
                    target_dir = dir_path.string();
                }
            }

            if (!std::filesystem::exists(dir_path)) {
                auto suggestions =
                    suggestion_utils::generate_cd_suggestions(target_dir, current_directory);
                ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "cd",
                                   target_dir + ": no such file or directory", suggestions};
                print_error(error);
                return 1;
            }
        }

        if (!std::filesystem::is_directory(dir_path)) {
            ErrorInfo error = {
                ErrorType::INVALID_ARGUMENT, "cd", target_dir + ": not a directory", {}};
            print_error(error);
            return 1;
        }

        std::string old_directory = current_directory;

        std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
        current_directory = canonical_path.string();

        if (chdir(current_directory.c_str()) != 0) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR, "cd", std::string(strerror(errno)), {}};
            print_error(error);
            return 1;
        }

        cjsh_env::set_shell_variable_value("PWD", current_directory);
        cjsh_env::set_shell_variable_value("OLDPWD", old_directory);

        previous_directory = old_directory;

        if (shell != nullptr && old_directory != current_directory) {
            shell->execute_hooks(HookType::Chpwd);
        }

        return 0;
    } catch (const std::filesystem::filesystem_error& e) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR, "cd", std::string(e.what()), {}};
        print_error(error);
        return 1;
    } catch (const std::exception& e) {
        ErrorInfo error = {
            ErrorType::RUNTIME_ERROR, "cd", "unexpected error: " + std::string(e.what()), {}};
        print_error(error);
        return 1;
    }
}
