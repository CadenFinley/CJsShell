#pragma once

#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <ctime>

#include "ai.h"
#include "main_loop.h"
#include "plugin.h"
#include "shell.h"
#include "theme.h"

const bool PRE_RELEASE = false;
const std::string pre_release_line = " (pre-release)";
// using semver.org principles MAJOR.MINOR.PATCH
const std::string c_version = "3.5.12" + (PRE_RELEASE ? pre_release_line : "");

#ifndef CJSH_GIT_HASH
#define CJSH_GIT_HASH "unknown"
#endif

extern bool g_debug_mode;
extern bool g_exit_flag;
extern std::string g_current_theme;
extern bool g_startup_active;

class Shell;
extern std::unique_ptr<Shell> g_shell;
extern std::unique_ptr<Theme> g_theme;
extern std::unique_ptr<Ai> g_ai;
extern std::unique_ptr<Plugin> g_plugin;
extern std::vector<std::string> g_startup_args;
extern std::vector<std::string> g_profile_startup_args;

namespace config {
extern bool login_mode;
extern bool interactive_mode;
extern bool force_interactive;
extern bool execute_command;
extern std::string cmd_to_execute;
extern bool plugins_enabled;
extern bool themes_enabled;
extern bool ai_enabled;
extern bool colors_enabled;
extern bool source_enabled;
extern bool completions_enabled;
extern bool syntax_highlighting_enabled;
extern bool smart_cd_enabled;
extern bool show_version;
extern bool show_help;
extern bool startup_test;
extern bool minimal_mode;
extern bool disable_custom_ls;
extern bool show_startup_time;
}  // namespace config

void initialize_colors();
void initialize_plugins();
void initialize_themes();
void initialize_ai();

void cleanup_resources();