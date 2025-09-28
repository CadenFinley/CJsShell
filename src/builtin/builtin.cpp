#include "builtin.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iomanip>

#include "ai_command.h"
#include "aihelp_command.h"
#include "alias_command.h"
#include "approot_command.h"
#include "cd_command.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
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
#include "source_command.h"
#include "startup_flag_command.h"
#include "style_def_command.h"
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

    // Initialize the bookmark database
    auto load_result = bookmark_database::g_bookmark_db.load();
    if (load_result.is_error()) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "bookmark",
                     "Failed to load bookmark database: " + load_result.error(),
                     {}});
    } else {
        // Clean up any invalid bookmarks after loading
        auto cleanup_result = bookmark_database::g_bookmark_db
                                  .cleanup_invalid_bookmarks_with_count();
        if (cleanup_result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "bookmark",
                         "Failed to cleanup invalid bookmarks: " +
                             cleanup_result.error(),
                         {}});
        } else {
            int removed_count = cleanup_result.value();
            if (removed_count > 0) {
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Pruned " << removed_count
                              << " invalid bookmark"
                              << (removed_count == 1 ? "" : "s")
                              << " on startup" << std::endl;
                }

                // Save the cleaned database if any bookmarks were removed
                auto save_result = bookmark_database::g_bookmark_db.save();
                if (save_result.is_error()) {
                    print_error({ErrorType::RUNTIME_ERROR,
                                 "bookmark",
                                 "Failed to save cleaned bookmark database: " +
                                     save_result.error(),
                                 {}});
                }
            }
        }
    }

    // Import existing bookmarks if any
    if (!directory_bookmarks.empty()) {
        auto import_result = bookmark_database::g_bookmark_db.import_from_map(
            directory_bookmarks);
        if (import_result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "bookmark",
                         "Failed to import existing bookmarks: " +
                             import_result.error(),
                         {}});
        } else {
            // Save the imported bookmarks
            auto save_result = bookmark_database::g_bookmark_db.save();
            if (save_result.is_error()) {
                print_error({ErrorType::RUNTIME_ERROR,
                             "bookmark",
                             "Failed to save imported bookmarks: " +
                                 save_result.error(),
                             {}});
            }
        }
    }

    builtins = {
        {"echo",
         [](const std::vector<std::string>& args) {
             return ::echo_command(args);
         }},
        {"printf",
         [](const std::vector<std::string>& args) {
             return ::printf_command(args);
         }},
        {"pwd",
         [](const std::vector<std::string>& args) {
             return ::pwd_command(args);
         }},
        {"ls",
         [this](const std::vector<std::string>& args) {
             return ::ls_command(args, shell);
         }},
        {"cd",
         [this](const std::vector<std::string>& args) {
             // args[0] == "cd"; optional directory operand at args[1]
             // Check for too many arguments (cd should accept at most 1
             // argument)
             if (args.size() > 2) {
                 ErrorInfo error = {ErrorType::INVALID_ARGUMENT,
                                    "cd",
                                    "too many arguments",
                                    {"Usage: cd [directory]"}};
                 print_error(error);
                 return 2;  // Misuse of shell builtin
             }
             if (config::smart_cd_enabled) {
                 return ::change_directory_smart(
                     args.size() > 1 ? args[1] : "", current_directory,
                     previous_directory, last_terminal_output_error);
             } else {
                 return ::change_directory(
                     args.size() > 1 ? args[1] : "", current_directory,
                     previous_directory, last_terminal_output_error);
             }
         }},
        {"local",
         [this](const std::vector<std::string>& args) {
             return ::local_command(args, shell);
         }},
        {"alias",
         [this](const std::vector<std::string>& args) {
             return ::alias_command(args, shell);
         }},
        {"export",
         [this](const std::vector<std::string>& args) {
             return ::export_command(args, shell);
         }},
        {"unalias",
         [this](const std::vector<std::string>& args) {
             return ::unalias_command(args, shell);
         }},
        {"unset",
         [this](const std::vector<std::string>& args) {
             return ::unset_command(args, shell);
         }},
        {"set",
         [this](const std::vector<std::string>& args) {
             return ::set_command(args, shell);
         }},
        {"shift",
         [this](const std::vector<std::string>& args) {
             return ::shift_command(args, shell);
         }},
        {"break",
         [](const std::vector<std::string>& args) {
             return ::break_command(args);
         }},
        {"continue",
         [](const std::vector<std::string>& args) {
             return ::continue_command(args);
         }},
        {"return",
         [](const std::vector<std::string>& args) {
             return ::return_command(args);
         }},
        {"ai",
         [this](const std::vector<std::string>& args) {
             return ::ai_command(args, this);
         }},
        {"source",
         [](const std::vector<std::string>& args) {
             return ::source_command(args);
         }},
        {"login-startup-arg",
         [](const std::vector<std::string>& args) {
             return ::startup_flag_command(args);
         }},
        {".",
         [](const std::vector<std::string>& args) {
             return ::source_command(args);
         }},
        {"theme",
         [](const std::vector<std::string>& args) {
             return ::theme_command(args);
         }},
        {"plugin",
         [](const std::vector<std::string>& args) {
             return ::plugin_command(args);
         }},
        {"help",
         [](const std::vector<std::string>&) { return ::help_command(); }},
        {"approot",
         [this](const std::vector<std::string>&) {
             return ::change_to_approot(current_directory, previous_directory,
                                        last_terminal_output_error);
         }},
        {"aihelp",
         [](const std::vector<std::string>& args) {
             return ::aihelp_command(args);
         }},
        {"version",
         [](const std::vector<std::string>& args) {
             return ::version_command(args);
         }},
        {"eval",
         [this](const std::vector<std::string>& args) {
             return ::eval_command(args, shell);
         }},
        {"syntax",
         [this](const std::vector<std::string>& args) {
             return ::syntax_command(args, shell);
         }},
        {"style_def",
         [](const std::vector<std::string>& args) {
             return ::style_def_command(args);
         }},
        {"history",
         [](const std::vector<std::string>& args) {
             return ::history_command(args);
         }},
        {"exit",
         [](const std::vector<std::string>& args) {
             return ::exit_command(args);
         }},
        {"quit",
         [](const std::vector<std::string>& args) {
             return ::exit_command(args);
         }},
        {"prompt_test",
         [](const std::vector<std::string>& args) {
             extern int prompt_test_command(const std::vector<std::string>&);
             return prompt_test_command(args);
         }},
        {"test",
         [](const std::vector<std::string>& args) {
             return ::test_command(args);
         }},
        {"[",
         [](const std::vector<std::string>& args) {
             return ::test_command(args);
         }},
        {"[[",
         [](const std::vector<std::string>& args) {
             return ::double_bracket_command(args);
         }},
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
        {"trap",
         [](const std::vector<std::string>& args) {
             return ::trap_command(args);
         }},
        {"jobs",
         [](const std::vector<std::string>& args) {
             return ::jobs_command(args);
         }},
        {"fg",
         [](const std::vector<std::string>& args) {
             return ::fg_command(args);
         }},
        {"bg",
         [](const std::vector<std::string>& args) {
             return ::bg_command(args);
         }},
        {"wait",
         [](const std::vector<std::string>& args) {
             return ::wait_command(args);
         }},
        {"kill",
         [](const std::vector<std::string>& args) {
             return ::kill_command(args);
         }},
        {"readonly",
         [this](const std::vector<std::string>& args) {
             return ::readonly_command(args, shell);
         }},
        {"read",
         [this](const std::vector<std::string>& args) {
             return ::read_command(args, shell);
         }},
        {"umask",
         [](const std::vector<std::string>& args) {
             return ::umask_command(args);
         }},
        {"getopts",
         [this](const std::vector<std::string>& args) {
             return ::getopts_command(args, shell);
         }},
        {"times",
         [](const std::vector<std::string>& args) {
             return ::times_command(args, nullptr);
         }},
        {"type",
         [this](const std::vector<std::string>& args) {
             return ::type_command(args, shell);
         }},
        {"which",
         [this](const std::vector<std::string>& args) {
             return ::which_command(args, shell);
         }},
        {"validate",
         [this](const std::vector<std::string>& args) {
             return ::validate_command(args, shell);
         }},
        {"hash",
         [](const std::vector<std::string>& args) {
             return ::hash_command(args, nullptr);
         }},
    };
}

Built_ins::~Built_ins() {
    // Save the bookmark database when the shell exits
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
            return ::change_directory_smart("", current_directory,
                                            previous_directory,
                                            last_terminal_output_error);
        }
        int status = it->second(args);
        return status;
    }
    auto suggestions = suggestion_utils::generate_command_suggestions(args[0]);

    if (g_debug_mode) {
        std::cerr << "DEBUG: Command not found in builtins: " << args[0]
                  << std::endl;
    }

    // Check if this command is in our cache but no longer exists (stale entry)
    if (cjsh_filesystem::is_executable_in_cache(args[0])) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Command '" << args[0]
                      << "' found in cache, checking if stale..." << std::endl;
        }
        // Double-check that it really doesn't exist in PATH
        std::string full_path =
            cjsh_filesystem::find_executable_in_path(args[0]);
        if (full_path.empty()) {
            if (g_debug_mode) {
                std::cerr << "DEBUG: Removing stale cache entry for command "
                             "not found: "
                          << args[0] << std::endl;
            }
            cjsh_filesystem::remove_executable_from_cache(args[0]);
        } else if (g_debug_mode) {
            std::cerr << "DEBUG: Command '" << args[0]
                      << "' found in PATH: " << full_path << std::endl;
        }
    } else if (g_debug_mode) {
        std::cerr << "DEBUG: Command '" << args[0] << "' not in cache"
                  << std::endl;
    }

    ErrorInfo error = {ErrorType::COMMAND_NOT_FOUND, args[0],
                       "command not found", suggestions};
    print_error(error);
    last_terminal_output_error = "cjsh: '" + args[0] + "': command not found";
    return 127;
}

int Built_ins::is_builtin_command(const std::string& cmd) const {
    if (cmd.empty()) {
        return 0;
    }

    // Special case for ls: check if custom ls should be used
    if (cmd == "ls") {
        // Check config flag first
        if (config::disable_custom_ls) {
            return 0;  // Don't treat as builtin
        }

        // Check TTY and interactive mode
        if (!isatty(STDOUT_FILENO)) {
            return 0;  // Don't treat as builtin when output is not a TTY
        }

        if (shell && !shell->get_interactive_mode()) {
            return 0;  // Don't treat as builtin in non-interactive mode
        }
    }

    return builtins.find(cmd) != builtins.end();
}

int Built_ins::do_ai_request(const std::string& prompt) {
    return ::ai_command({"ai", prompt}, this);
}

void Built_ins::add_directory_bookmark(const std::string& dir_path) {
    std::filesystem::path path(dir_path);
    std::string basename = path.filename().string();
    if (!basename.empty() && basename != "." && basename != "..") {
        auto result =
            bookmark_database::g_bookmark_db.add_bookmark(basename, dir_path);
        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "bookmark",
                         "Failed to add bookmark: " + result.error(),
                         {}});
        } else {
            // Also update the legacy map for backward compatibility
            directory_bookmarks[basename] = dir_path;
        }
    }
}

std::string Built_ins::find_bookmark_path(
    const std::string& bookmark_name) const {
    // First try the new database
    auto bookmark_path =
        bookmark_database::g_bookmark_db.get_bookmark(bookmark_name);
    if (bookmark_path.has_value()) {
        return bookmark_path.value();
    }

    // Fall back to legacy map
    auto it = directory_bookmarks.find(bookmark_name);
    if (it != directory_bookmarks.end()) {
        return it->second;
    }
    return "";
}

const std::unordered_map<std::string, std::string>&
Built_ins::get_directory_bookmarks() const {
    // Update the legacy map from the database for backward compatibility
    const_cast<Built_ins*>(this)->directory_bookmarks =
        bookmark_database::g_bookmark_db.get_all_bookmarks();
    return directory_bookmarks;
}