#pragma once

#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <ctime>

#include "ai.h"
#include "plugin.h"
#include "shell.h"
#include "theme.h"

const bool PRE_RELEASE = true;
// using semver.org principles MAJOR.MINOR.PATCH
const std::string c_version = "3.1.5";
const std::string pre_release_line = " (pre-release)";

extern bool g_debug_mode;
extern bool g_title_line;
extern struct termios g_original_termios;
extern bool g_terminal_state_saved;
extern int g_shell_terminal;
extern pid_t g_shell_pgid;
extern struct termios g_shell_tmodes;
extern bool g_job_control_enabled;
extern bool g_exit_flag;
extern std::string g_cached_version;
extern std::string g_current_theme;
extern std::string title_line;
extern std::string created_line;
extern bool g_startup_active;

class Shell;
extern std::unique_ptr<Shell> g_shell;
extern std::unique_ptr<Theme> g_theme;
extern std::unique_ptr<Ai> g_ai;
extern std::unique_ptr<Plugin> g_plugin;
extern std::vector<std::string> g_startup_args;
void reprint_prompt();
void cleanup_resources();