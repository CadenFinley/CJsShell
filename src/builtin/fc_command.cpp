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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell.h"

namespace {

std::vector<std::string> read_history_entries() {
    cjsh_filesystem::initialize_cjsh_directories();

    auto read_result =
        cjsh_filesystem::read_file_content(cjsh_filesystem::g_cjsh_history_path().string());

    std::vector<std::string> entries;
    entries.reserve(256);

    if (read_result.is_error()) {
        return entries;
    }

    std::stringstream content_stream(read_result.value());
    std::string line;

    while (std::getline(content_stream, line)) {
        if (line.empty()) {
            continue;
        }
        if (!line.empty() && line[0] == '#') {
            continue;
        }
        entries.push_back(line);
    }

    return entries;
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
        if (!is_fc_history_entry(entries[i])) {
            return i;
        }
    }
    return -1;
}

bool parse_history_index(const std::string& arg, int history_size, int& result) {
    try {
        int index = std::stoi(arg);

        if (index < 0) {
            index = history_size + index;
        }

        if (index < 0 || index >= history_size) {
            return false;
        }

        result = index;
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
            std::cout << entries[i] << '\n';
        }
    } else {
        for (int i = first; i <= last; ++i) {
            if (show_numbers) {
                std::cout << std::setw(5) << i << "  ";
            }
            std::cout << entries[i] << '\n';
        }
    }

    return 0;
}

std::string get_editor() {
    const char* fcedit = std::getenv("FCEDIT");
    if (fcedit && fcedit[0] != '\0') {
        return std::string(fcedit);
    }

    const char* editor = std::getenv("EDITOR");
    if (editor && editor[0] != '\0') {
        return std::string(editor);
    }

    return "nano";
}

int edit_and_execute_string(const std::string& initial_content, const std::string& editor,
                            Shell* shell) {
    cjsh_filesystem::initialize_cjsh_directories();
    const auto& temp_dir = cjsh_filesystem::g_cjsh_cache_path();
    auto temp_file = temp_dir / ("fc_edit_" + std::to_string(getpid()) + ".sh");

    std::ofstream out(temp_file);
    if (!out) {
        print_error({ErrorType::RUNTIME_ERROR, "fc", "Failed to create temporary file", {}});
        return 1;
    }

    out << initial_content << '\n';
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

    cjsh_filesystem::initialize_cjsh_directories();
    const auto& temp_dir = cjsh_filesystem::g_cjsh_cache_path();
    auto temp_file = temp_dir / ("fc_edit_" + std::to_string(getpid()) + ".sh");

    std::ofstream out(temp_file);
    if (!out) {
        print_error({ErrorType::RUNTIME_ERROR, "fc", "Failed to create temporary file", {}});
        return 1;
    }

    for (int i = first; i <= last; ++i) {
        out << entries[i] << '\n';
    }
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

int substitute_and_execute(const std::vector<std::string>& entries, const std::string& old_str,
                           const std::string& new_str, const std::string& pattern, Shell* shell,
                           int default_index) {
    int target_idx = default_index;

    if (!pattern.empty()) {
        target_idx = -1;
        for (int i = static_cast<int>(entries.size()) - 1; i >= 0; --i) {
            if (entries[i].find(pattern) == 0) {
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

    std::string command = entries[target_idx];

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
    int first_idx = -1;
    int last_idx = -1;

    size_t i = 1;
    while (i < args.size()) {
        const std::string& arg = args[i];

        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: fc [-e editor] [-lnr] [first [last]]\n";
            std::cout << "       fc -s [old=new] [command]\n";
            std::cout << "       fc -c command_string\n";
            std::cout << "\n";
            std::cout << "Fix Command - Edit and re-execute commands from history.\n";
            std::cout << "\n";
            std::cout << "Options:\n";
            std::cout
                << "  -e editor   Use specified editor (default: $FCEDIT, $EDITOR, or nano)\n";
            std::cout << "  -l          List commands instead of editing\n";
            std::cout << "  -n          Suppress line numbers when listing\n";
            std::cout << "  -r          Reverse order of commands when listing\n";
            std::cout << "  -s          Re-execute command with optional substitution\n";
            std::cout << "  -c string   Open editor with the provided string\n";
            std::cout << "\n";
            std::cout << "Arguments:\n";
            std::cout << "  first       First command to edit/list (default: previous command)\n";
            std::cout << "  last        Last command to edit/list (default: same as first)\n";
            std::cout << "  old=new     String substitution (for -s option)\n";
            std::cout << "  command     Command pattern to match (for -s option)\n";
            std::cout << "\n";
            std::cout << "Examples:\n";
            std::cout << "  fc              Edit the previous command\n";
            std::cout << "  fc -l           List recent history\n";
            std::cout << "  fc -l 10 20     List commands 10 through 20\n";
            std::cout << "  fc 53           Edit command 53\n";
            std::cout << "  fc -e nano      Edit previous command with nano\n";
            std::cout << "  fc -s           Re-execute the previous command\n";
            std::cout << "  fc -s echo      Re-execute most recent 'echo' command\n";
            std::cout
                << "  fc -s old=new   Re-execute previous command, replacing 'old' with 'new'\n";
            std::cout << "  fc -c 'echo hello'  Open editor with 'echo hello' as initial content\n";
            return 0;
        } else if (arg == "-l") {
            list_mode = true;
            ++i;
        } else if (arg == "-n") {
            show_numbers = false;
            ++i;
        } else if (arg == "-r") {
            reverse_order = true;
            ++i;
        } else if (arg == "-e") {
            if (i + 1 >= args.size()) {
                print_error(
                    {ErrorType::INVALID_ARGUMENT, "fc", "-e requires an editor argument", {}});
                return 1;
            }
            editor = args[i + 1];
            i += 2;
        } else if (arg == "-c" || arg == "--command") {
            if (i + 1 >= args.size()) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "fc",
                             "-c requires a command string argument",
                             {}});
                return 1;
            }
            command_mode = true;
            initial_command = args[i + 1];
            i += 2;
        } else if (arg == "-s") {
            substitute_mode = true;
            ++i;
        } else if (arg[0] == '-') {
            print_error({ErrorType::INVALID_ARGUMENT, "fc", "Unknown option: " + arg, {}});
            return 1;
        } else {
            if (substitute_mode) {
                size_t eq_pos = arg.find('=');
                if (eq_pos != std::string::npos) {
                    old_pattern = arg.substr(0, eq_pos);
                    new_pattern = arg.substr(eq_pos + 1);
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
                if (first_idx == -1) {
                    first_idx = i;
                    ++i;
                } else if (last_idx == -1) {
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

    if (first_idx != -1) {
        if (!parse_history_index(args[first_idx], entries.size(), first)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "fc",
                         "Invalid history index: " + args[first_idx],
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

    if (last_idx != -1) {
        if (!parse_history_index(args[last_idx], entries.size(), last)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "fc",
                         "Invalid history index: " + args[last_idx],
                         {}});
            return 1;
        }
    } else if (first_idx != -1) {
        last = first;
    }

    if (list_mode) {
        return list_history(entries, first, last, show_numbers, reverse_order);
    } else {
        return edit_and_execute(entries, first, last, editor, shell);
    }
}
