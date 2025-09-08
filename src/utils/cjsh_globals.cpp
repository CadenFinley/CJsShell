#include <atomic>

#include "cjsh.h"
bool g_first_boot = false;
bool g_debug_mode = false;
bool g_title_line = true;
struct termios g_original_termios;
bool g_terminal_state_saved = false;
int g_shell_terminal;
pid_t g_shell_pgid = 0;
struct termios g_shell_tmodes;
bool g_job_control_enabled = false;
bool g_exit_flag = false;
std::string g_cached_version = "";
std::string g_current_theme = "";
std::string pre_release_line =
    std::string("-") + "\033[1;31m" + "PRERELEASE" + c_reset_color;
std::string title_line = " CJ's Shell v" + c_version +
                         (PRE_RELEASE ? pre_release_line : "") +
                         " - Caden J Finley (c) 2025";
std::string created_line = " Created 2025 @ " + c_title_color +
                           "Abilene Christian University" + c_reset_color;
Ai* g_ai = nullptr;
std::unique_ptr<Shell> g_shell = nullptr;
Theme* g_theme = nullptr;
Plugin* g_plugin = nullptr;
std::vector<std::string> g_startup_args;

bool g_startup_active = true;
