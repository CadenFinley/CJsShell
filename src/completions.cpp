#include "completions.h"
#include "shell.h"
#include "cjsh_filesystem.h"
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

// External variables from main.h
extern Shell* g_shell;
extern bool g_debug_mode;

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
    
    while (std::getline(history_file, line)) {
        if (line.rfind(prefix, 0) == 0 && line != prefix) {
            std::string suffix = line.substr(prefix_len);
            if (!ic_add_completion(cenv, suffix.c_str())) return;
        }
    }
}

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
    ic_complete_filename(cenv, prefix, '/', nullptr, nullptr);
}

void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix) {
    cjsh_command_completer(cenv, prefix);
    cjsh_history_completer(cenv, prefix);
    cjsh_filename_completer(cenv, prefix);
}

void initialize_completion_system() {
    if (g_debug_mode) std::cerr << "DEBUG: Initializing completion system" << std::endl;
    ic_set_default_completer(cjsh_default_completer, NULL);
    ic_enable_auto_tab(true);
}
