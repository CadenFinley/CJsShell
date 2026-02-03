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

#include "error_out.h"
#include "shell.h"
#include "suggestion_utils.h"

int change_directory(const std::string& dir, std::string& current_directory,
                     std::string& previous_directory, Shell* shell) {
    std::string target_dir = dir;

    if (target_dir.empty() || target_dir == "~") {
        const char* home_dir = getenv("HOME");
        if (!home_dir) {
            ErrorInfo error = {
                ErrorType::RUNTIME_ERROR, "cd", "HOME environment variable is not set", {}};
            print_error(error);
            return 1;
        }
        target_dir = home_dir;
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
        const char* home_dir = getenv("HOME");
        if (!home_dir) {
            ErrorInfo error = {ErrorType::RUNTIME_ERROR,
                               "cd",
                               "Cannot expand '~' - HOME environment variable is not set",
                               {}};
            print_error(error);
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
            auto suggestions =
                suggestion_utils::generate_cd_suggestions(target_dir, current_directory);
            ErrorInfo error = {ErrorType::FILE_NOT_FOUND, "cd",
                               target_dir + ": no such file or directory", suggestions};
            print_error(error);
            return 1;
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

        setenv("PWD", current_directory.c_str(), 1);
        setenv("OLDPWD", old_directory.c_str(), 1);

        previous_directory = old_directory;

        if (shell != nullptr && old_directory != current_directory) {
            shell->execute_hooks("chpwd");
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
