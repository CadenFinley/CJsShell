#include "completions.h"
#include "shell.h"
#include "cjsh_filesystem.h"
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include "main.h"

std::map<std::string, int> g_completion_frequency;

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix) {
    size_t prefix_len = std::strlen(prefix);
    auto cmds = g_shell->get_available_commands();
    for (const auto& cmd : cmds) {
        if (cmd.rfind(prefix, 0) == 0) {
            std::string suffix = cmd.substr(prefix_len);
            if (!ic_add_completion(cenv, suffix.c_str())) return;
        }
    }
}

void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix) {
    size_t prefix_len = std::strlen(prefix);
    if (prefix_len == 0) return;
    std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path);
    if (!history_file.is_open()) return;
    
    std::string line;
    std::vector<std::pair<std::string, int>> matches;

    while (std::getline(history_file, line)) {
        if (line.rfind(prefix, 0) == 0 && line != prefix) {
            if (g_completion_frequency.find(line) == g_completion_frequency.end()) {
                g_completion_frequency[line] = 1;
            }
            matches.push_back({line, g_completion_frequency[line]});
        }
    }

    std::sort(matches.begin(), matches.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& match : matches) {
        std::string suffix = match.first.substr(prefix_len);
        if (!ic_add_completion(cenv, suffix.c_str())) return;
    }
}

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
    ic_complete_filename(cenv, prefix, '/', nullptr, nullptr);
}

void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix) {
    cjsh_history_completer(cenv, prefix);
    cjsh_command_completer(cenv, prefix);
    cjsh_filename_completer(cenv, prefix);
}

void initialize_completion_system() {
    if (g_debug_mode) std::cerr << "DEBUG: Initializing completion system" << std::endl;

    ic_style_def("completion", "bold color=#00FFFF");
    ic_style_def("completion-preview", "bold color=#FFFF00");
    ic_style_def("completion-select", "bold reverse color=#FFFFFF");
    
    ic_set_default_completer(cjsh_default_completer, NULL);
    ic_enable_auto_tab(false);
    ic_enable_completion_preview(true);
    ic_enable_hint(true);
    ic_set_hint_delay(10);
    ic_enable_highlight(true);
    ic_enable_history_duplicates(false);
    ic_enable_inline_help(false);
    ic_enable_multiline_indent(false);
    ic_set_prompt_marker("", NULL);
    ic_set_history(cjsh_filesystem::g_cjsh_history_path.c_str(), -1);
}

void update_completion_frequency(const std::string& command) {
    if (!command.empty()) {
        g_completion_frequency[command]++;
    }
}
