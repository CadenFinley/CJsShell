#pragma once

#include "ai.h"
#include "theme.h"
#include "plugin.h"
#include "shell.h"
#include "update.h"
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <ctime>
#include <atomic>

const std::string c_version = "2.1.11";
const std::string c_github_url = "https://github.com/CadenFinley/CJsShell";
const std::string c_update_url = "https://api.github.com/repos/cadenfinley/CJsShell/releases/latest";
const pid_t c_pid = getpid();
const std::string c_pid_str = std::to_string(getpid());
const std::string c_reset_color = "\033[0m";
const std::string c_title_color = "\033[1;35m";


extern bool g_debug_mode;
extern bool g_cached_update;
extern bool g_check_updates;
extern bool g_title_line;
extern bool g_silent_update_check;
extern struct termios g_original_termios;
extern bool g_terminal_state_saved;
extern int g_shell_terminal;
extern pid_t g_shell_pgid;
extern struct termios g_shell_tmodes;
extern bool g_job_control_enabled;
extern std::atomic_bool g_exit_flag;
extern time_t g_last_update_check;
extern int g_update_check_interval;
extern std::string g_cached_version;
extern std::string g_last_updated;
extern bool g_first_boot;
extern std::string g_current_theme;
extern std::string title_line;
extern std::string created_line;
extern Ai* g_ai;

class Shell;
extern Shell* g_shell;
extern Theme* g_theme;
extern Plugin* g_plugin;
extern std::vector<std::string> g_startup_args;

std::string get_colorized_splash();
int main(int argc, char *argv[]);
bool init_login_filesystem();
bool init_interactive_filesystem();
void main_process_loop();
void notify_plugins(std::string trigger, std::string data);
void process_source_file();
void process_profile_file();
void create_profile_file();
void create_source_file();
bool is_parent_process_alive();
void setup_signal_handlers();
void save_terminal_state();
void restore_terminal_state();
void setup_job_control();
void setup_environment_variables();
void initialize_login_environment();
bool is_shell_script_construct(const std::string& line);
void process_shell_scripts_in_config();
bool parse_and_set_env_var(const std::string& line);
void prepare_shell_signal_environment();
std::string get_current_time_string();
void handle_command_error(const std::string& command, const std::string& error_message);
