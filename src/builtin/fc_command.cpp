/*
  fc_command.cpp

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

#include "fc_command.h"

#include "builtin_help.h"
#include "builtin_option_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "cjsh_filesystem.h"
#include "error_out.h"
#include "history_file_utils.h"
#include "parser_utils.h"
#include "shell.h"
#include "shell_env.h"

namespace {

const std::vector<std::string>& fc_help_lines() {
    static const std::vector<std::string> lines = {
        "Usage: fc [-e editor] [-lnr] [first [last]]",
        "       fc -s [old=new] [command]",
        "       fc -c command_string",
        "",
        "Fix Command - Edit and re-execute commands from history.",
        "",
        "Options:",
        "  -e editor   Use specified editor (default: $FCEDIT, $EDITOR, or nano)",
        "  -l          List commands instead of editing",
        "  -n          Suppress line numbers when listing",
        "  -r          Reverse order of commands when listing",
        "  -s          Re-execute command with optional substitution",
        "  -c string   Open editor with the provided string",
        "",
        "Arguments:",
        "  first       First command to edit/list (default: previous command)",
        "  last        Last command to edit/list (default: same as first)",
        "  old=new     String substitution (for -s option)",
        "  command     Command pattern to match (for -s option)",
        "",
        "Examples:",
        "  fc              Edit the previous command",
        "  fc -l           List recent history",
        "  fc -l 10 20     List commands 10 through 20",
        "  fc 53           Edit command 53",
        "  fc -e nano      Edit previous command with nano",
        "  fc -s           Re-execute the previous command",
        "  fc -s echo      Re-execute most recent 'echo' command",
        "  fc -s old=new   Re-execute previous command, replacing 'old' with 'new'",
        "  fc -c 'echo hello'  Open editor with 'echo hello' as initial content"};
    return lines;
}

std::vector<std::string> read_history_entries() {
    cjsh_filesystem::initialize_cjsh_directories();
    return history_file_utils::read_history_entries(
        cjsh_filesystem::g_cjsh_history_path().string());
}

bool is_fc_history_entry(const std::string& entry) {
    if (entry.length() < 2) {
        return entry == "fc";
    }

    if (entry.compare(0, 2, "fc") != 0) {
        return false;
    }

    if (entry.length() == 2) {
        return true;
    }

    unsigned char next = static_cast<unsigned char>(entry[2]);
    if (std::isspace(next) != 0) {
        return true;
    }

    switch (entry[2]) {
        case ';':
        case '&':
        case '|':
        case '>':
        case '<':
        case ')':
        case '(':
            return true;
        default:
            return false;
    }
}

int find_last_non_fc_index(const std::vector<std::string>& entries) {
    for (int i = static_cast<int>(entries.size()) - 1; i >= 0; --i) {
        if (!is_fc_history_entry(entries[static_cast<size_t>(i)])) {
            return i;
        }
    }
    return -1;
}

bool parse_history_index(const std::string& arg, size_t history_size, int& result) {
    try {
        if (history_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return false;
        }

        long long index = std::stoll(arg);
        long long history_size_ll = static_cast<long long>(history_size);

        if (index < 0) {
            index = history_size_ll + index;
        }

        if (index < 0 || index >= history_size_ll) {
            return false;
        }

        result = static_cast<int>(index);
        return true;
    } catch (...) {
        return false;
    }
}

int list_history(const std::vector<std::string>& entries, int first, int last, bool show_numbers,
                 bool reverse_order) {
    if (first < 0 || first >= static_cast<int>(entries.size())) {
        first = 0;
    }
    if (last < 0 || last >= static_cast<int>(entries.size())) {
        last = static_cast<int>(entries.size()) - 1;
    }

    if (first > last) {
        std::swap(first, last);
    }

    if (reverse_order) {
        for (int i = last; i >= first; --i) {
            if (show_numbers) {
                std::cout << std::setw(5) << i << "  ";
            }
            std::cout << entries[static_cast<size_t>(i)] << '\n';
        }
    } else {
        for (int i = first; i <= last; ++i) {
            if (show_numbers) {
                std::cout << std::setw(5) << i << "  ";
            }
            std::cout << entries[static_cast<size_t>(i)] << '\n';
        }
    }

    return 0;
}

std::string get_editor() {
    std::string fcedit = cjsh_env::get_shell_variable_value("FCEDIT");
    if (!fcedit.empty()) {
        return fcedit;
    }

    std::string editor = cjsh_env::get_shell_variable_value("EDITOR");
    if (!editor.empty()) {
        return editor;
    }

    return "nano";
}

int edit_and_execute_content(const std::string& initial_content, const std::string& editor,
                             Shell* shell) {
    cjsh_filesystem::initialize_cjsh_directories();
    const auto& temp_dir = cjsh_filesystem::g_cjsh_cache_path();
    auto temp_file = temp_dir / ("fc_edit_" + std::to_string(getpid()) + ".sh");

    std::ofstream out(temp_file);
    if (!out) {
        print_error({ErrorType::RUNTIME_ERROR, "fc", "Failed to create temporary file", {}});
        return 1;
    }

    out << initial_content;
    out.close();

    std::vector<std::string> editor_args = {editor, temp_file.string()};
    int editor_exit_code = shell->execute_command(editor_args, false);

    if (editor_exit_code != 0) {
        std::filesystem::remove(temp_file);
        return editor_exit_code;
    }

    auto read_result = cjsh_filesystem::read_file_content(temp_file.string());
    std::filesystem::remove(temp_file);

    if (read_result.is_error()) {
        print_error({ErrorType::RUNTIME_ERROR, "fc", "Failed to read edited commands", {}});
        return 1;
    }

    const std::string& edited_content = read_result.value();

    if (edited_content.empty()) {
        return 0;
    }

    std::istringstream iss(edited_content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[0] != '#') {
            std::cout << line << '\n';
        }
    }

    int exit_code = shell->execute(edited_content);

    return exit_code;
}

int edit_and_execute_string(const std::string& initial_content, const std::string& editor,
                            Shell* shell) {
    return edit_and_execute_content(initial_content + '\n', editor, shell);
}

int edit_and_execute(const std::vector<std::string>& entries, int first, int last,
                     const std::string& editor, Shell* shell) {
    if (first < 0 || first >= static_cast<int>(entries.size())) {
        first = static_cast<int>(entries.size()) - 1;
    }
    if (last < 0 || last >= static_cast<int>(entries.size())) {
        last = first;
    }

    if (first > last) {
        std::swap(first, last);
    }

    std::ostringstream initial_content;
    for (int i = first; i <= last; ++i) {
        initial_content << entries[static_cast<size_t>(i)] << '\n';
    }

    return edit_and_execute_content(initial_content.str(), editor, shell);
}

int substitute_and_execute(const std::vector<std::string>& entries, const std::string& old_str,
                           const std::string& new_str, const std::string& pattern, Shell* shell,
                           int default_index) {
    int target_idx = default_index;

    if (!pattern.empty()) {
        target_idx = -1;
        for (int i = static_cast<int>(entries.size()) - 1; i >= 0; --i) {
            if (entries[static_cast<size_t>(i)].find(pattern) == 0) {
                target_idx = i;
                break;
            }
        }

        if (target_idx < 0) {
            print_error({ErrorType::RUNTIME_ERROR,
                         "fc",
                         "No command in history starting with: " + pattern,
                         {}});
            return 1;
        }
    }

    if (target_idx < 0 || target_idx >= static_cast<int>(entries.size())) {
        print_error({ErrorType::RUNTIME_ERROR, "fc", "No commands in history", {}});
        return 1;
    }

    std::string command = entries[static_cast<size_t>(target_idx)];

    if (!old_str.empty()) {
        size_t pos = command.find(old_str);
        if (pos != std::string::npos) {
            command.replace(pos, old_str.length(), new_str);
        }
    }

    std::cout << command << '\n';

    int exit_code = shell->execute(command);

    return exit_code;
}

}  // namespace

int fc_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, fc_help_lines(), BuiltinHelpScanMode::AnyArgument)) {
        return 0;
    }

    bool list_mode = false;
    bool substitute_mode = false;
    bool command_mode = false;
    bool show_numbers = true;
    bool reverse_order = false;
    std::string editor = get_editor();
    std::string old_pattern;
    std::string new_pattern;
    std::string command_pattern;
    std::string initial_command;
    std::optional<size_t> first_idx;
    std::optional<size_t> last_idx;

    size_t start_index = 1;
    std::vector<BuiltinParsedShortOption> parsed_options;
    const bool options_ok = builtin_parse_short_options_ex(
        args, start_index, "fc",
        [](char option) {
            return option == 'l' || option == 'n' || option == 'r' || option == 'e' ||
                   option == 's' || option == 'c';
        },
        [](char option) { return option == 'e' || option == 'c'; }, parsed_options);
    if (!options_ok) {
        return 1;
    }

    for (const auto& option : parsed_options) {
        switch (option.option) {
            case 'l':
                list_mode = true;
                break;
            case 'n':
                show_numbers = false;
                break;
            case 'r':
                reverse_order = true;
                break;
            case 'e':
                editor = option.value.value_or("");
                break;
            case 'c':
                command_mode = true;
                initial_command = option.value.value_or("");
                break;
            case 's':
                substitute_mode = true;
                break;
            default:
                break;
        }
    }

    size_t i = start_index;
    while (i < args.size()) {
        const std::string& arg = args[i];

        if (arg == "--command") {
            if (i + 1 >= args.size()) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "fc",
                             "--command requires a command string argument",
                             {}});
                return 1;
            }
            command_mode = true;
            initial_command = args[i + 1];
            i += 2;
        } else if (!arg.empty() && arg[0] == '-') {
            print_error({ErrorType::INVALID_ARGUMENT, "fc", "Unknown option: " + arg, {}});
            return 1;
        } else {
            if (substitute_mode) {
                if (split_on_first_equals(arg, old_pattern, new_pattern, false)) {
                    ++i;
                } else if (command_pattern.empty()) {
                    command_pattern = arg;
                    ++i;
                } else {
                    print_error(
                        {ErrorType::INVALID_ARGUMENT, "fc", "Too many arguments for -s", {}});
                    return 1;
                }
            } else {
                if (!first_idx.has_value()) {
                    first_idx = i;
                    ++i;
                } else if (!last_idx.has_value()) {
                    last_idx = i;
                    ++i;
                } else {
                    print_error({ErrorType::INVALID_ARGUMENT, "fc", "Too many arguments", {}});
                    return 1;
                }
            }
        }
    }

    std::vector<std::string> entries = read_history_entries();
    int last_non_fc_index = find_last_non_fc_index(entries);

    if (command_mode) {
        return edit_and_execute_string(initial_command, editor, shell);
    }

    if (entries.empty()) {
        print_error({ErrorType::RUNTIME_ERROR, "fc", "No commands in history", {}});
        return 1;
    }

    if (substitute_mode) {
        if (last_non_fc_index < 0 && command_pattern.empty()) {
            print_error({ErrorType::RUNTIME_ERROR, "fc", "No commands in history", {}});
            return 1;
        }
        return substitute_and_execute(entries, old_pattern, new_pattern, command_pattern, shell,
                                      last_non_fc_index);
    }

    int first = -1;
    int last = -1;

    if (first_idx.has_value()) {
        if (!parse_history_index(args[*first_idx], entries.size(), first)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "fc",
                         "Invalid history index: " + args[*first_idx],
                         {}});
            return 1;
        }
    } else {
        if (list_mode) {
            int default_last =
                (last_non_fc_index >= 0) ? last_non_fc_index : static_cast<int>(entries.size()) - 1;
            if (default_last < 0) {
                print_error({ErrorType::RUNTIME_ERROR, "fc", "No commands in history", {}});
                return 1;
            }
            first = std::max(0, default_last - 15);
            last = default_last;
        } else {
            if (last_non_fc_index < 0) {
                print_error({ErrorType::RUNTIME_ERROR, "fc", "No commands in history", {}});
                return 1;
            }
            first = last_non_fc_index;
            last = first;
        }
    }

    if (last_idx.has_value()) {
        if (!parse_history_index(args[*last_idx], entries.size(), last)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "fc",
                         "Invalid history index: " + args[*last_idx],
                         {}});
            return 1;
        }
    } else if (first_idx.has_value()) {
        last = first;
    }

    if (list_mode) {
        return list_history(entries, first, last, show_numbers, reverse_order);
    } else {
        return edit_and_execute(entries, first, last, editor, shell);
    }
}
