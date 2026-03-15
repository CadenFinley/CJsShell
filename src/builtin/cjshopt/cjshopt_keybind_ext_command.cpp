/*
  cjshopt_keybind_ext_command.cpp

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

#include "cjshopt_command.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "isocline.h"
#include "shell_env.h"

namespace {
std::unordered_map<ic_keycode_t, std::string> g_custom_keybindings;
}  // namespace

std::string get_custom_keybinding(ic_keycode_t key) {
    auto it = g_custom_keybindings.find(key);
    if (it != g_custom_keybindings.end()) {
        return it->second;
    }
    return "";
}

bool has_custom_keybinding(ic_keycode_t key) {
    return g_custom_keybindings.find(key) != g_custom_keybindings.end();
}

void set_custom_keybinding(ic_keycode_t key, const std::string& command) {
    g_custom_keybindings[key] = command;
}

void clear_custom_keybinding(ic_keycode_t key) {
    g_custom_keybindings.erase(key);
}

void clear_all_custom_keybindings() {
    g_custom_keybindings.clear();
}

namespace {
int keybind_ext_list_command() {
    if (cjsh_env::startup_active()) {
        return 0;
    }

    if (g_custom_keybindings.empty()) {
        std::cout << "No custom command keybindings are currently defined.\n";
        std::cout << "To bind a key to a command, add 'cjshopt keybind ext set <key> <command>' "
                     "to your ~/.cjshrc file.\n";
        return 0;
    }

    std::cout << "Custom command keybindings:\n\n";
    std::cout << std::left << std::setw(20) << "Key" << "Command\n";
    std::cout << std::string(60, '-') << '\n';

    std::vector<std::pair<ic_keycode_t, std::string>> sorted_bindings(g_custom_keybindings.begin(),
                                                                      g_custom_keybindings.end());
    std::sort(sorted_bindings.begin(), sorted_bindings.end());

    for (const auto& [key, command] : sorted_bindings) {
        char buffer[64];
        if (ic_format_key_spec(key, buffer, sizeof(buffer))) {
            std::cout << std::left << std::setw(20) << buffer << command << '\n';
        }
    }

    std::cout << "\nThese bindings are defined in your configuration files.\n";
    std::cout << "To modify them, edit your ~/.cjshrc file.\n";

    return 0;
}

int keybind_ext_set_command(const std::vector<std::string>& args) {
    if (args.size() < 5) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "keybind ext",
                     "set requires a key specification and a command",
                     {"Usage: keybind ext set <key_spec> <command>",
                      "Example: keybind ext set 'ctrl-g' 'echo Hello from ctrl-g!'"}});
        return 1;
    }

    const std::string& key_spec = args[3];

    std::string command;
    for (size_t i = 4; i < args.size(); ++i) {
        if (i > 4) {
            command += " ";
        }
        command += args[i];
    }

    ic_keycode_t key_code = 0;
    if (!ic_parse_key_spec(key_spec.c_str(), &key_code)) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "keybind ext",
                     "Invalid key specification '" + key_spec + "'",
                     {"Use standard key spec format like 'ctrl-g', 'alt-h', 'F5', etc."}});
        return 1;
    }

    ic_key_action_t existing_action;
    if (ic_get_key_binding(key_code, &existing_action)) {
        if (existing_action != IC_KEY_ACTION_RUNOFF) {
            const char* action_name = ic_key_action_name(existing_action);
            std::string bound_name = action_name ? action_name : "(unknown action)";
            print_error({ErrorType::INVALID_ARGUMENT,
                         ErrorSeverity::WARNING,
                         "keybind ext",
                         "Key '" + key_spec + "' is already bound to '" + bound_name +
                             "' and will be overridden.",
                         {"Use 'keybind ext list' to review custom bindings."}});

            ic_clear_key_binding(key_code);
        }
    }

    if (!ic_bind_key(key_code, IC_KEY_ACTION_RUNOFF)) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "keybind ext",
                     "Failed to bind key specification '" + key_spec + "'",
                     {}});
        return 1;
    }

    set_custom_keybinding(key_code, command);

    if (!cjsh_env::startup_active()) {
        std::cout << "Bound key '" << key_spec << "' to command: " << command << '\n';
        std::cout << "Add `cjshopt keybind ext set '" << key_spec << "' '" << command
                  << "'` to your ~/.cjshrc to persist this change.\n";
    }

    return 0;
}

int keybind_ext_clear_command(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "keybind ext",
                     "clear requires at least one key specification",
                     {"Usage: keybind ext clear <key_spec> [<key_spec> ...]",
                      "Example: keybind ext clear 'ctrl-g' 'alt-h'"}});
        return 1;
    }

    std::vector<std::string> cleared;
    std::vector<std::string> not_found;

    for (size_t i = 3; i < args.size(); ++i) {
        const std::string& key_spec = args[i];

        ic_keycode_t key_code = 0;
        if (!ic_parse_key_spec(key_spec.c_str(), &key_code)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "keybind ext",
                         "Invalid key specification '" + key_spec + "'",
                         {}});
            continue;
        }

        if (has_custom_keybinding(key_code)) {
            clear_custom_keybinding(key_code);
            ic_clear_key_binding(key_code);
            cleared.push_back(key_spec);
        } else {
            not_found.push_back(key_spec);
        }
    }

    if (!cjsh_env::startup_active()) {
        if (!cleared.empty()) {
            std::cout << "Cleared custom command binding(s) for: ";
            for (size_t i = 0; i < cleared.size(); ++i) {
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << cleared[i];
            }
            std::cout << '\n';
        }
        if (!not_found.empty()) {
            std::cout << "No custom command binding found for: ";
            for (size_t i = 0; i < not_found.size(); ++i) {
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << not_found[i];
            }
            std::cout << '\n';
        }
    }

    return cleared.empty() ? 1 : 0;
}

int keybind_ext_reset_command() {
    for (const auto& [key, _] : g_custom_keybindings) {
        ic_clear_key_binding(key);
    }
    clear_all_custom_keybindings();

    if (!cjsh_env::startup_active()) {
        std::cout << "All custom command keybindings cleared.\n";
    }

    return 0;
}
}  // namespace

int keybind_ext_command(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "keybind ext",
                     "Missing subcommand",
                     {"Usage: keybind ext <subcommand> [...]", "",
                      "Subcommands:", "  list              Show all custom command keybindings",
                      "  set <key> <cmd>   Bind a key to execute a command",
                      "  clear <key>...    Remove custom command bindings for specified key(s)",
                      "  reset             Clear all custom command keybindings", "",
                      "Examples:", "  keybind ext set 'ctrl-g' 'echo Hello!'",
                      "  keybind ext set 'F5' 'clear'", "  keybind ext list",
                      "  keybind ext clear 'ctrl-g'", "  keybind ext reset"}});
        return 1;
    }

    const std::string& subcommand = args[2];

    if (subcommand == "list") {
        return keybind_ext_list_command();
    }

    if (subcommand == "set") {
        return keybind_ext_set_command(args);
    }

    if (subcommand == "clear") {
        return keybind_ext_clear_command(args);
    }

    if (subcommand == "reset") {
        return keybind_ext_reset_command();
    }

    print_error({ErrorType::INVALID_ARGUMENT,
                 "keybind ext",
                 "Unknown subcommand '" + subcommand + "'",
                 {"Available subcommands: list, set, clear, reset"}});
    return 1;
}
