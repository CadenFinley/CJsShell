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
#include <optional>

#include "builtin_help.h"
#include "cjsh_filesystem.h"
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

    suggestion_utils::CdLookupContext context =
        suggestion_utils::build_cd_lookup_context(target_dir, current_dir);

    std::error_code ec;

    auto raw_similar =
        suggestion_utils::find_similar_entries(context.lookup_fragment, context.base_dir, 2);
    if (raw_similar.size() != 1) {
        return std::nullopt;
    }

    std::filesystem::path candidate_path = context.search_base / raw_similar.front();

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
    std::string requested_dir = dir;

    if (target_dir.empty() || target_dir == "~") {
        if (!cjsh_env::shell_variable_is_set("HOME")) {
            ErrorInfo error = {
                ErrorType::RUNTIME_ERROR, "cd", "HOME environment variable is not set", {}};
            print_error(error);
            return 1;
        }
        target_dir = "~";
        requested_dir = "~";
    }

    if (target_dir == "-") {
        if (previous_directory.empty()) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR, "cd", "No previous directory", {}};
            print_error(error);
            return 1;
        }
    }

    if (target_dir.rfind("-/", 0) == 0 && previous_directory.empty()) {
        ErrorInfo error = {ErrorType::RUNTIME_ERROR, "cd", "No previous directory", {}};
        print_error(error);
        return 1;
    }

    std::filesystem::path dir_path;

    try {
        dir_path = cjsh_filesystem::expand_shell_path_token(target_dir, current_directory,
                                                            previous_directory);
        if (dir_path.empty()) {
            ErrorInfo error = {
                ErrorType::FILE_NOT_FOUND, "cd", requested_dir + ": no such file or directory", {}};
            print_error(error);
            return 1;
        }

        if (!std::filesystem::exists(dir_path)) {
            if (config::smart_cd_enabled && !config::minimal_mode && !config::secure_mode &&
                !config::posix_mode) {
                auto smart_target = resolve_smart_cd_target(requested_dir, current_directory);
                if (smart_target.has_value()) {
                    dir_path = *smart_target;
                    target_dir = dir_path.string();
                }
            }

            if (!std::filesystem::exists(dir_path)) {
                auto suggestions =
                    suggestion_utils::generate_cd_suggestions(requested_dir, current_directory);
                ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "cd",
                                   requested_dir + ": no such file or directory", suggestions};
                print_error(error);
                return 1;
            }
        }

        if (!std::filesystem::is_directory(dir_path)) {
            ErrorInfo error = {
                ErrorType::INVALID_ARGUMENT, "cd", requested_dir + ": not a directory", {}};
            print_error(error);
            return 1;
        }

        std::string old_directory = current_directory;

        std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
        current_directory = canonical_path.string();

        if (chdir(current_directory.c_str()) != 0) {
            print_error_errno({ErrorType::RUNTIME_ERROR, "cd", "chdir", {}});
            return 1;
        }

        cjsh_env::set_shell_variable_value("PWD", current_directory);
        cjsh_env::set_shell_variable_value("OLDPWD", old_directory);

        previous_directory = old_directory;

        if (shell != nullptr && old_directory != current_directory && !config::posix_mode) {
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
