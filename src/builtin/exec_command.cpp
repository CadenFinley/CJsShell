/*
  exec_command.cpp

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

#include "exec_command.h"

#include "builtin_help.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>

#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell.h"

int exec_command(const std::vector<std::string>& args, Shell* shell,
                 std::string& last_terminal_output_error) {
    if (builtin_handle_help(
            args, {"Usage: exec [COMMAND [ARG ...]]", "Replace the current shell with COMMAND.",
                   "If COMMAND is omitted, apply redirections to the shell."})) {
        return 0;
    }
    if (args.size() <= 1) {
        return 0;
    }

    std::vector<std::string> exec_args;
    bool has_fd_operations = false;

    auto record_error = [&](const ErrorInfo& info) {
        last_terminal_output_error = info.message;
        print_error(info);
    };

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg.size() > 1 && std::isdigit(static_cast<unsigned char>(arg[0]))) {
            size_t fd_end = 0;
            while (fd_end < arg.size() && std::isdigit(static_cast<unsigned char>(arg[fd_end]))) {
                fd_end++;
            }

            if (fd_end > 0 && fd_end < arg.size()) {
                int fd_num = std::stoi(arg.substr(0, fd_end));
                std::string op = arg.substr(fd_end);

                if (op == "<" && i + 1 < args.size()) {
                    const std::string& filename = args[i + 1];
                    auto redirect_result = cjsh_filesystem::redirect_fd(filename, fd_num, O_RDONLY);
                    if (redirect_result.is_error()) {
                        record_error(
                            {ErrorType::RUNTIME_ERROR, "exec", redirect_result.error(), {}});
                        return 1;
                    }
                    has_fd_operations = true;
                    i++;
                    continue;
                } else if (op == ">" && i + 1 < args.size()) {
                    const std::string& filename = args[i + 1];
                    auto redirect_result = cjsh_filesystem::redirect_fd(
                        filename, fd_num, O_WRONLY | O_CREAT | O_TRUNC);
                    if (redirect_result.is_error()) {
                        record_error(
                            {ErrorType::RUNTIME_ERROR, "exec", redirect_result.error(), {}});
                        return 1;
                    }
                    has_fd_operations = true;
                    i++;
                    continue;
                } else if (op.find("<&") == 0 && op.size() > 2) {
                    try {
                        int src_fd = std::stoi(op.substr(2));
                        auto dup_result = cjsh_filesystem::safe_dup2(src_fd, fd_num);
                        if (dup_result.is_error()) {
                            record_error(
                                {ErrorType::RUNTIME_ERROR, "exec", dup_result.error(), {}});
                            return 1;
                        }
                        has_fd_operations = true;
                        continue;
                    } catch (const std::exception&) {
                    }
                } else if (op.find(">&") == 0 && op.size() > 2) {
                    try {
                        int src_fd = std::stoi(op.substr(2));
                        auto dup_result = cjsh_filesystem::safe_dup2(src_fd, fd_num);
                        if (dup_result.is_error()) {
                            record_error(
                                {ErrorType::RUNTIME_ERROR, "exec", dup_result.error(), {}});
                            return 1;
                        }
                        has_fd_operations = true;
                        continue;
                    } catch (const std::exception&) {
                    }
                }
            }
        }

        exec_args.push_back(arg);
    }

    if (has_fd_operations && exec_args.empty()) {
        return 0;
    }

    if (!exec_args.empty() && shell) {
        return shell->execute_command(exec_args, false);
    }

    return 0;
}
