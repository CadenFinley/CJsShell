/*
  builtin.cpp

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

#include "builtin.h"

#include "builtin_help.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>

#include "abbr_command.h"
#include "alias_command.h"
#include "bg_command.h"
#include "cd_command.h"
#include "cjsh.h"
#include "cjshopt_command.h"
#include "command_command.h"
#include "disown_command.h"
#include "double_bracket_command.h"
#include "echo_command.h"
#include "error_out.h"
#include "eval_command.h"
#include "exec_command.h"
#include "exit_command.h"
#include "export_command.h"
#include "false_command.h"
#include "fc_command.h"
#include "fg_command.h"
#include "generate_completions_command.h"
#include "getopts_command.h"
#include "hash_command.h"
#include "help_command.h"
#include "history_command.h"
#include "hook_command.h"
#include "if_command.h"
#include "internal_brace_group_command.h"
#include "internal_subshell_command.h"
#include "jobname_command.h"
#include "jobs_command.h"
#include "kill_command.h"
#include "local_command.h"
#include "loop_control_commands.h"
#include "printf_command.h"
#include "pwd_command.h"
#include "read_command.h"
#include "readonly_command.h"
#include "set_command.h"
#include "shell.h"
#include "source_command.h"
#include "suggestion_utils.h"
#include "test_command.h"
#include "times_command.h"
#include "trap_command.h"
#include "true_command.h"
#include "type_command.h"
#include "ulimit_command.h"
#include "umask_command.h"
#include "version_command.h"
#include "wait_command.h"
#include "which_command.h"
#include "widget_command.h"

Built_ins::Built_ins() : shell(nullptr) {
    builtins = {
        {"echo", [](const std::vector<std::string>& args) { return ::echo_command(args); }},
        {"printf", [](const std::vector<std::string>& args) { return ::printf_command(args); }},
        {"pwd", [](const std::vector<std::string>& args) { return ::pwd_command(args); }},
        {"true", [](const std::vector<std::string>&) { return ::true_command(); }},
        {"false", [](const std::vector<std::string>&) { return ::false_command(); }},
        {"cd",
         [this](const std::vector<std::string>& args) {
             if (builtin_handle_help(args, {"Usage: cd [DIR]", "Change the current directory.",
                                            "Use '-' to switch to the previous directory."})) {
                 return 0;
             }
             if (args.size() > 2) {
                 ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                                    "cd",
                                    "too many arguments",
                                    {"Usage: cd [directory]"}};
                 print_error(error);
                 return 2;
             }
             return ::change_directory(args.size() > 1 ? args[1] : "", current_directory,
                                       previous_directory, last_terminal_output_error, shell);
         }},
        {"local",
         [this](const std::vector<std::string>& args) { return ::local_command(args, shell); }},
        {"alias",
         [this](const std::vector<std::string>& args) { return ::alias_command(args, shell); }},
        {"abbr",
         [this](const std::vector<std::string>& args) { return ::abbr_command(args, shell); }},
        {"abbreviate",
         [this](const std::vector<std::string>& args) { return ::abbr_command(args, shell); }},
        {"export",
         [this](const std::vector<std::string>& args) { return ::export_command(args, shell); }},
        {"unalias",
         [this](const std::vector<std::string>& args) { return ::unalias_command(args, shell); }},
        {"unabbr",
         [this](const std::vector<std::string>& args) { return ::unabbr_command(args, shell); }},
        {"unabbreviate",
         [this](const std::vector<std::string>& args) { return ::unabbr_command(args, shell); }},
        {"unset",
         [this](const std::vector<std::string>& args) { return ::unset_command(args, shell); }},
        {"set",
         [this](const std::vector<std::string>& args) { return ::set_command(args, shell); }},
        {"shift",
         [this](const std::vector<std::string>& args) { return ::shift_command(args, shell); }},
        {"break", [](const std::vector<std::string>& args) { return ::break_command(args); }},
        {"continue", [](const std::vector<std::string>& args) { return ::continue_command(args); }},
        {"return", [](const std::vector<std::string>& args) { return ::return_command(args); }},
        {"source", [](const std::vector<std::string>& args) { return ::source_command(args); }},
        {".", [](const std::vector<std::string>& args) { return ::source_command(args); }},
        {"help",
         [](const std::vector<std::string>& args) {
             if (builtin_handle_help(args,
                                     {"Usage: help", "Display the CJSH command reference."})) {
                 return 0;
             }
             return ::help_command();
         }},
        {"hash", [](const std::vector<std::string>& args) { return ::hash_command(args); }},
        {"version", [](const std::vector<std::string>& args) { return ::version_command(args); }},
        {"eval",
         [this](const std::vector<std::string>& args) { return ::eval_command(args, shell); }},
        {"history", [](const std::vector<std::string>& args) { return ::history_command(args); }},
        {"fc", [this](const std::vector<std::string>& args) { return ::fc_command(args, shell); }},
        {"exit", [](const std::vector<std::string>& args) { return ::exit_command(args); }},
        {"quit", [](const std::vector<std::string>& args) { return ::exit_command(args); }},
        {"test", [](const std::vector<std::string>& args) { return ::test_command(args); }},
        {"[", [](const std::vector<std::string>& args) { return ::test_command(args); }},
        {"[[", [](const std::vector<std::string>& args) { return ::double_bracket_command(args); }},
        {"exec",
         [this](const std::vector<std::string>& args) {
             return ::exec_command(args, shell, last_terminal_output_error);
         }},
        {":", [](const std::vector<std::string>&) { return 0; }},
        {"if",
         [this](const std::vector<std::string>& args) {
             return ::if_command(args, shell, last_terminal_output_error);
         }},
        {"__INTERNAL_SUBSHELL__",
         [this](const std::vector<std::string>& args) {
             return internal_subshell_command(args, shell);
         }},
        {"__INTERNAL_BRACE_GROUP__",
         [this](const std::vector<std::string>& args) {
             return internal_brace_group_command(args, shell);
         }},
        {"trap", [](const std::vector<std::string>& args) { return ::trap_command(args); }},
        {"jobs", [](const std::vector<std::string>& args) { return ::jobs_command(args); }},
        {"jobname", [](const std::vector<std::string>& args) { return ::jobname_command(args); }},
        {"fg", [](const std::vector<std::string>& args) { return ::fg_command(args); }},
        {"bg", [](const std::vector<std::string>& args) { return ::bg_command(args); }},
        {"wait", [](const std::vector<std::string>& args) { return ::wait_command(args); }},
        {"kill", [](const std::vector<std::string>& args) { return ::kill_command(args); }},
        {"disown", [](const std::vector<std::string>& args) { return ::disown_command(args); }},
        {"readonly", [](const std::vector<std::string>& args) { return ::readonly_command(args); }},
        {"read",
         [this](const std::vector<std::string>& args) { return ::read_command(args, shell); }},
        {"umask", [](const std::vector<std::string>& args) { return ::umask_command(args); }},
        {"ulimit", [](const std::vector<std::string>& args) { return ::ulimit_command(args); }},

        {"getopts",
         [this](const std::vector<std::string>& args) { return ::getopts_command(args, shell); }},
        {"times", [](const std::vector<std::string>& args) { return ::times_command(args); }},
        {"type",
         [this](const std::vector<std::string>& args) { return ::type_command(args, shell); }},
        {"which",
         [this](const std::vector<std::string>& args) { return ::which_command(args, shell); }},
        {"generate-completions",
         [this](const std::vector<std::string>& args) {
             return ::generate_completions_command(args, shell);
         }},
        {"hook",
         [this](const std::vector<std::string>& args) { return ::hook_command(args, shell); }},
        {"command",
         [this](const std::vector<std::string>& args) { return ::command_command(args, shell); }},
        {"cjsh-widget",
         [](const std::vector<std::string>& args) { return ::widget_builtin(args); }},
        {"builtin",
         [this](const std::vector<std::string>& args) {
             if (builtin_handle_help(
                     args, {"Usage: builtin COMMAND [ARGS...]",
                            "Invoke a builtin command bypassing functions and PATH lookup."})) {
                 return 0;
             }
             if (args.size() < 2) {
                 ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                                    "builtin",
                                    "missing command operand",
                                    {"Usage: builtin <command> [args...]"}};
                 print_error(error);
                 last_terminal_output_error = "cjsh: builtin: missing command operand";
                 return 2;
             }

             const std::string& target_command = args[1];
             if (target_command == "builtin") {
                 ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                                    "builtin",
                                    "cannot invoke builtin recursively",
                                    {"Usage: builtin <command> [args...]"}};
                 print_error(error);
                 last_terminal_output_error = "cjsh: builtin: cannot invoke builtin recursively";
                 return 2;
             }

             auto builtin_it = builtins.find(target_command);
             if (builtin_it == builtins.end()) {
                 ErrorInfo error = {ErrorType::COMMAND_NOT_FOUND,
                                    "builtin",
                                    "'" + target_command + "' is not a builtin command",
                                    {"Use 'help' to list available builtins"}};
                 print_error(error);
                 last_terminal_output_error =
                     "cjsh: builtin: " + target_command + ": not a builtin command";
                 return 1;
             }

             std::vector<std::string> forwarded_args(args.begin() + 1, args.end());
             return builtin_it->second(forwarded_args);
         }},
        {"cjshopt", [](const std::vector<std::string>& args) { return ::cjshopt_command(args); }},
        {"true", [](const std::vector<std::string>&) { return true_command(); }},
        {"false", [](const std::vector<std::string>&) { return false_command(); }},
    };
}

Built_ins::~Built_ins() {
}

void Built_ins::set_shell(Shell* shell_ptr) {
    shell = shell_ptr;
}

std::string Built_ins::get_current_directory() const {
    return current_directory;
}

std::string Built_ins::get_previous_directory() const {
    return previous_directory;
}

void Built_ins::set_current_directory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        current_directory = cwd;
    } else {
        current_directory = "/";
    }
}

std::vector<std::string> Built_ins::get_builtin_commands() const {
    std::vector<std::string> names;
    names.reserve(builtins.size());
    for (const auto& kv : builtins) {
        names.push_back(kv.first);
    }
    return names;
}

std::string Built_ins::get_last_error() const {
    return last_terminal_output_error;
}

int Built_ins::builtin_command(const std::vector<std::string>& args) {
    if (args.empty())
        return 1;

    auto it = builtins.find(args[0]);
    if (it != builtins.end()) {
        if (args[0] == "cd" && args.size() == 1) {
            return ::change_directory("", current_directory, previous_directory,
                                      last_terminal_output_error, shell);
        }
        int status = it->second(args);
        return status;
    }
    auto suggestions = suggestion_utils::generate_command_suggestions(args[0]);

    ErrorInfo error = {ErrorType::COMMAND_NOT_FOUND, args[0], "command not found", suggestions};
    print_error(error);
    last_terminal_output_error = "cjsh: '" + args[0] + "': command not found";
    return 127;
}

int Built_ins::is_builtin_command(const std::string& cmd) const {
    if (cmd.empty()) {
        return 0;
    }

    return builtins.find(cmd) != builtins.end();
}
