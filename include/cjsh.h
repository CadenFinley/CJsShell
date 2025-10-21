#pragma once

#include <memory>
#include <string>
#include <vector>

class Shell;
class Theme;

const bool PRE_RELEASE = false;

constexpr const char* c_version_base = "3.10.4";

extern std::string g_cached_version;

inline std::string get_version() {
    if (g_cached_version.empty()) {
        g_cached_version = std::string(c_version_base) + (PRE_RELEASE ? " (pre-release)" : "");
    }
    return g_cached_version;
}

#ifndef CJSH_GIT_HASH
#define CJSH_GIT_HASH "unknown"
#endif

extern bool g_exit_flag;
extern std::string g_current_theme;
extern bool g_startup_active;

class Shell;
extern std::unique_ptr<Shell> g_shell;
extern std::unique_ptr<Theme> g_theme;
extern std::vector<std::string> g_startup_args;
extern std::vector<std::string> g_profile_startup_args;

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
}  // namespace config

void initialize_themes();
void cleanup_resources();
