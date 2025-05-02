#include "main.h"
#include <atomic>
bool g_first_boot = false;
bool g_debug_mode = false;
bool g_cached_update = false;
bool g_check_updates = true;
bool g_title_line = true;
bool g_silent_update_check = true;
struct termios g_original_termios;
bool g_terminal_state_saved = false;
int g_shell_terminal;
pid_t g_shell_pgid = 0;
struct termios g_shell_tmodes;
bool g_job_control_enabled = false;
std::atomic_bool g_exit_flag{false};
time_t g_last_update_check = 0;
int g_update_check_interval = 86400;
std::string g_cached_version = "";
std::string g_last_updated = "";
std::string g_current_theme = "default";
std::string title_line = " CJ's Shell v" + c_version + " - Caden J Finley (c) 2025";
std::string created_line = " Created 2025 @ " + c_title_color + "Abilene Christian University" + c_reset_color;
Ai* g_ai = nullptr;
Shell* g_shell = nullptr;
Theme* g_theme = nullptr;
Plugin* g_plugin = nullptr;
std::vector<std::string> g_startup_args;
