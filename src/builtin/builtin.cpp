#include "builtin.h"

#include "builtin_help.h"

#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "ai_command.h"
#include "aihelp_command.h"
#include "alias_command.h"
#include "cd_command.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "cjshopt_command.h"
#include "double_bracket_command.h"
#include "echo_command.h"
#include "error_out.h"
#include "eval_command.h"
#include "exec_command.h"
#include "exit_command.h"
#include "export_command.h"
#include "getopts_command.h"
#include "hash_command.h"
#include "help_command.h"
#include "history_command.h"
#include "if_command.h"
#include "internal_subshell_command.h"
#include "job_control.h"
#include "local_command.h"
#include "loop_control_commands.h"
#include "ls_command.h"
#include "plugin_command.h"
#include "printf_command.h"
#include "prompt_test_command.h"
#include "pwd_command.h"
#include "read_command.h"
#include "readonly_command.h"
#include "set_command.h"
#include "shell.h"
#include "source_command.h"
#include "suggestion_utils.h"
#include "syntax_command.h"
#include "test_command.h"
#include "theme_command.h"
#include "times_command.h"
#include "trap_command.h"
#include "type_command.h"
#include "umask_command.h"
#include "utils/bookmark_database.h"
#include "validate_command.h"
#include "version_command.h"
#include "which_command.h"

Built_ins::Built_ins() : shell(nullptr) {
    builtins.reserve(64);

    auto load_result = bookmark_database::g_bookmark_db.load();
    if (load_result.is_error()) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "bookmark",
                     "Failed to load bookmark database: " + load_result.error(),
                     {}});
    } else {
        auto cleanup_result =
            bookmark_database::g_bookmark_db.cleanup_invalid_bookmarks_with_count();
        if (cleanup_result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "bookmark",
                         "Failed to cleanup invalid bookmarks: " + cleanup_result.error(),
                         {}});
        } else {
            int removed_count = cleanup_result.value();
            if (removed_count > 0) {
                auto save_result = bookmark_database::g_bookmark_db.save();
                if (save_result.is_error()) {
                    print_error({ErrorType::RUNTIME_ERROR,
                                 "bookmark",
                                 "Failed to save cleaned bookmark database: " + save_result.error(),
                                 {}});
                }
            }
        }
    }

    if (!directory_bookmarks.empty()) {
        auto import_result = bookmark_database::g_bookmark_db.import_from_map(directory_bookmarks);
        if (import_result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "bookmark",
                         "Failed to import existing bookmarks: " + import_result.error(),
                         {}});
        } else {
            auto save_result = bookmark_database::g_bookmark_db.save();
            if (save_result.is_error()) {
                print_error({ErrorType::RUNTIME_ERROR,
                             "bookmark",
                             "Failed to save imported bookmarks: " + save_result.error(),
                             {}});
            }
        }
    }

    builtins = {
        {"echo", [](const std::vector<std::string>& args) { return ::echo_command(args); }},
        {"printf", [](const std::vector<std::string>& args) { return ::printf_command(args); }},
        {"pwd", [](const std::vector<std::string>& args) { return ::pwd_command(args); }},
        {"ls", [this](const std::vector<std::string>& args) { return ::ls_command(args, shell); }},
        {"cd",
         [this](const std::vector<std::string>& args) {
             if (builtin_handle_help(args,
                                     {"Usage: cd [DIR]",
                                      "Change the current directory.",
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
             if (config::smart_cd_enabled) {
                 return ::change_directory_smart(args.size() > 1 ? args[1] : "", current_directory,
                                                 previous_directory, last_terminal_output_error);
             } else {
                 return ::change_directory(args.size() > 1 ? args[1] : "", current_directory,
                                           previous_directory, last_terminal_output_error);
             }
         }},
        {"local",
         [this](const std::vector<std::string>& args) { return ::local_command(args, shell); }},
        {"alias",
         [this](const std::vector<std::string>& args) { return ::alias_command(args, shell); }},
        {"export",
         [this](const std::vector<std::string>& args) { return ::export_command(args, shell); }},
        {"unalias",
         [this](const std::vector<std::string>& args) { return ::unalias_command(args, shell); }},
        {"unset",
         [this](const std::vector<std::string>& args) { return ::unset_command(args, shell); }},
        {"set",
         [this](const std::vector<std::string>& args) { return ::set_command(args, shell); }},
        {"shift",
         [this](const std::vector<std::string>& args) { return ::shift_command(args, shell); }},
        {"break", [](const std::vector<std::string>& args) { return ::break_command(args); }},
        {"continue", [](const std::vector<std::string>& args) { return ::continue_command(args); }},
        {"return", [](const std::vector<std::string>& args) { return ::return_command(args); }},
        {"ai", [this](const std::vector<std::string>& args) { return ::ai_command(args, this); }},
        {"source", [](const std::vector<std::string>& args) { return ::source_command(args); }},
        {".", [](const std::vector<std::string>& args) { return ::source_command(args); }},
        {"theme", [](const std::vector<std::string>& args) { return ::theme_command(args); }},
        {"plugin", [](const std::vector<std::string>& args) { return ::plugin_command(args); }},
        {"help",
         [](const std::vector<std::string>& args) {
             if (builtin_handle_help(args,
                                     {"Usage: help",
                                      "Display the CJSH command reference."})) {
                 return 0;
             }
             return ::help_command();
         }},
        {"aihelp", [](const std::vector<std::string>& args) { return ::aihelp_command(args); }},
        {"version", [](const std::vector<std::string>& args) { return ::version_command(args); }},
        {"eval",
         [this](const std::vector<std::string>& args) { return ::eval_command(args, shell); }},
        {"syntax",
         [this](const std::vector<std::string>& args) { return ::syntax_command(args, shell); }},
        {"history", [](const std::vector<std::string>& args) { return ::history_command(args); }},
        {"exit", [](const std::vector<std::string>& args) { return ::exit_command(args); }},
        {"quit", [](const std::vector<std::string>& args) { return ::exit_command(args); }},
        {"prompt_test",
         [](const std::vector<std::string>& args) {
             extern int prompt_test_command(const std::vector<std::string>&);
             return prompt_test_command(args);
         }},
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
        {"trap", [](const std::vector<std::string>& args) { return ::trap_command(args); }},
        {"jobs", [](const std::vector<std::string>& args) { return ::jobs_command(args); }},
        {"fg", [](const std::vector<std::string>& args) { return ::fg_command(args); }},
        {"bg", [](const std::vector<std::string>& args) { return ::bg_command(args); }},
        {"wait", [](const std::vector<std::string>& args) { return ::wait_command(args); }},
        {"kill", [](const std::vector<std::string>& args) { return ::kill_command(args); }},
        {"readonly",
         [this](const std::vector<std::string>& args) { return ::readonly_command(args, shell); }},
        {"read",
         [this](const std::vector<std::string>& args) { return ::read_command(args, shell); }},
        {"umask", [](const std::vector<std::string>& args) { return ::umask_command(args); }},
        {"getopts",
         [this](const std::vector<std::string>& args) { return ::getopts_command(args, shell); }},
        {"times",
         [](const std::vector<std::string>& args) { return ::times_command(args, nullptr); }},
        {"type",
         [this](const std::vector<std::string>& args) { return ::type_command(args, shell); }},
        {"which",
         [this](const std::vector<std::string>& args) { return ::which_command(args, shell); }},
        {"validate",
         [this](const std::vector<std::string>& args) { return ::validate_command(args, shell); }},
        {"hash",
         [](const std::vector<std::string>& args) { return ::hash_command(args, nullptr); }},
        {"builtin",
         [this](const std::vector<std::string>& args) {
             if (builtin_handle_help(args,
                                     {"Usage: builtin COMMAND [ARGS...]",
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
    };
}

Built_ins::~Built_ins() {
    auto save_result = bookmark_database::g_bookmark_db.save();
    if (save_result.is_error()) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "bookmark",
                     "Failed to save bookmark database: " + save_result.error(),
                     {}});
    }
}

int Built_ins::builtin_command(const std::vector<std::string>& args) {
    if (args.empty())
        return 1;

    auto it = builtins.find(args[0]);
    if (it != builtins.end()) {
        if (args[0] == "cd" && args.size() == 1) {
            return ::change_directory_smart("", current_directory, previous_directory,
                                            last_terminal_output_error);
        }
        int status = it->second(args);
        return status;
    }
    auto suggestions = suggestion_utils::generate_command_suggestions(args[0]);

    if (cjsh_filesystem::is_executable_in_cache(args[0])) {
        std::string full_path = cjsh_filesystem::find_executable_in_path(args[0]);
        if (full_path.empty()) {
            cjsh_filesystem::remove_executable_from_cache(args[0]);
        }
    }

    ErrorInfo error = {ErrorType::COMMAND_NOT_FOUND, args[0], "command not found", suggestions};
    print_error(error);
    last_terminal_output_error = "cjsh: '" + args[0] + "': command not found";
    return 127;
}

int Built_ins::is_builtin_command(const std::string& cmd) const {
    if (cmd.empty()) {
        return 0;
    }

    if (cmd == "ls") {
        if (config::disable_custom_ls) {
            return 0;
        }

        if (!isatty(STDOUT_FILENO)) {
            return 0;
        }

        if (shell && !shell->get_interactive_mode()) {
            return 0;
        }
    }

    return builtins.find(cmd) != builtins.end();
}

int Built_ins::do_ai_request(const std::string& prompt) {
    return ::ai_command({"ai", "chat", prompt}, this);
}

void Built_ins::add_directory_bookmark(const std::string& dir_path) {
    std::filesystem::path path(dir_path);
    std::string basename = path.filename().string();
    if (!basename.empty() && basename != "." && basename != "..") {
        auto result = bookmark_database::g_bookmark_db.add_bookmark(basename, dir_path);
        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "bookmark",
                         "Failed to add bookmark: " + result.error(),
                         {}});
        } else {
            directory_bookmarks[basename] = dir_path;
        }
    }
}

std::string Built_ins::find_bookmark_path(const std::string& bookmark_name) const {
    auto bookmark_path = bookmark_database::g_bookmark_db.get_bookmark(bookmark_name);
    if (bookmark_path.has_value()) {
        return bookmark_path.value();
    }

    auto it = directory_bookmarks.find(bookmark_name);
    if (it != directory_bookmarks.end()) {
        return it->second;
    }
    return "";
}

const std::unordered_map<std::string, std::string>& Built_ins::get_directory_bookmarks() const {
    const_cast<Built_ins*>(this)->directory_bookmarks =
        bookmark_database::g_bookmark_db.get_all_bookmarks();
    return directory_bookmarks;
}