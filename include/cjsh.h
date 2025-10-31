#pragma once

#include <memory>
#include <string>
#include <vector>

class Shell;

const bool PRE_RELEASE = false;
constexpr const char* c_version_base = "3.11.0";

inline std::string get_version() {
    static std::string cached_version =
        std::string(c_version_base) + (PRE_RELEASE ? " (pre-release)" : "");
    return cached_version;
}

#ifndef CJSH_GIT_HASH
#define CJSH_GIT_HASH "unknown"
#endif

extern bool g_exit_flag;
extern bool g_startup_active;

class Shell;
extern std::unique_ptr<Shell> g_shell;

inline std::vector<std::string>& startup_args() {
    static std::vector<std::string> args;
    return args;
}

inline std::vector<std::string>& profile_startup_args() {
    static std::vector<std::string> args;
    return args;
}

namespace config {
extern bool login_mode;
extern bool interactive_mode;
extern bool force_interactive;
extern bool execute_command;
extern std::string cmd_to_execute;
extern bool themes_enabled;
extern bool colors_enabled;
extern bool source_enabled;
extern bool completions_enabled;
extern bool syntax_highlighting_enabled;
extern bool smart_cd_enabled;
extern bool show_version;
extern bool show_help;
extern bool startup_test;
extern bool minimal_mode;
extern bool show_startup_time;
extern bool secure_mode;
extern bool show_title_line;
extern bool no_prompt;
extern bool history_expansion_enabled;
extern bool posix_mode;

void set_posix_mode(bool enable);
bool is_posix_mode();
}  // namespace config

void cleanup_resources();
