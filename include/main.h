#pragma once

#include "ai.h"
#include "shell.h"
#include "theme.h"
#include "plugin.h"
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <ctime>
#include <atomic>

// constants
const std::string c_version = "2.1.0.0";
const std::string c_github_url = "https://github.com/CadenFinley/CJsShell";
const std::string c_update_url = "https://api.github.com/repos/cadenfinley/CJsShell/releases/latest";
const pid_t c_pid = getpid();
const std::string c_pid_str = std::to_string(getpid());

// constant colors
const std::string c_reset_color = "\033[0m";
const std::string c_title_color = "\033[1;35m";

// globals
extern bool g_debug_mode;
extern bool g_silent_startup;
extern bool g_cached_update;
extern bool g_source;
extern bool g_check_updates;
extern bool g_title_line;
extern bool g_menu_terminal;
extern bool g_silent_update_check;
extern struct termios g_original_termios;
extern bool g_terminal_state_saved;
extern int g_shell_terminal;
extern pid_t g_shell_pgid;
extern struct termios g_shell_tmodes;
extern bool g_job_control_enabled;
extern std::atomic_bool g_exit_flag;

extern time_t g_last_update_check;
extern int g_update_check_interval; // 24 hours
extern std::string g_cached_version;
extern std::string g_last_updated;
extern bool g_first_boot;  // New variable to track first boot

extern std::vector<std::string> g_startup_commands;

// theme name the theme manger will load this
extern std::string g_current_theme;

// misc
extern std::string title_line;
extern std::string created_line;

// objects
extern Ai* g_ai;
extern Shell* g_shell;
extern Theme* g_theme;
extern Plugin* g_plugin;

const std::string c_cjsh_splash = 
    "   ______       __   _____    __  __\n"
    "  / ____/      / /  / ___/   / / / /\n"
    " / /      __  / /   \\__ \\   / /_/ / \n"
    "/ /___   / /_/ /   ___/ /  / __  /  \n"
    "\\____/   \\____/   /____/  /_/ /_/   \n"
    "  CJ's Shell v" + c_version;
std::string get_colorized_splash();

int main(int argc, char *argv[]);
bool init_login_filesystem();
bool init_interactive_filesystem();
void main_process_loop();
void notify_plugins(std::string trigger, std::string data);
void process_source_file();
void process_config_file();
void create_config_file();
void create_source_file();
bool is_parent_process_alive();
void parent_process_watchdog();
void setup_signal_handlers();
void save_terminal_state();
void restore_terminal_state();
void setup_job_control();
void setup_environment_variables();
void initialize_login_environment();
bool is_shell_script_construct(const std::string& line);
void process_shell_scripts_in_config();
bool parse_and_set_env_var(const std::string& line);

// using json = nlohmann::json;

void startup_update_process();
bool check_for_update();
bool load_update_cache();
void save_update_cache(bool update_available, const std::string& latest_version);
bool execute_update_if_available(bool update_available);
bool should_check_for_updates();
bool download_latest_release();
void display_changelog(const std::string& changelog_path);
std::string get_current_time_string();
bool is_newer_version(const std::string& latest, const std::string& current);
bool is_first_boot();
void mark_first_boot_complete();

// Add a function to handle command execution errors gracefully
void handle_command_error(const std::string& command, const std::string& error_message);
