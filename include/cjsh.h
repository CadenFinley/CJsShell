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
#include "update.h"

const bool PRE_RELEASE = true;
// using semver.org principles MAJOR.MINOR.PATCHs
const std::string c_version = "3.0.0";
const pid_t c_pid = getpid();
const std::string c_pid_str = std::to_string(getpid());
const std::string c_reset_color = "\033[0m";
const std::string c_title_color = "\033[1;35m";

extern std::string pre_release_line;
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
extern bool g_exit_flag;
extern time_t g_last_update_check;
extern int g_update_check_interval;
extern std::string g_cached_version;
extern std::string g_last_updated;
extern bool g_first_boot;
extern std::string g_current_theme;
extern std::string title_line;
extern std::string created_line;
extern Ai* g_ai;
extern bool g_startup_active;

class Shell;
extern std::unique_ptr<Shell> g_shell;
extern Theme* g_theme;
extern Plugin* g_plugin;
extern std::vector<std::string> g_startup_args;

std::string get_colorized_splash();
int main(int argc, char* argv[]);
bool init_login_filesystem();
bool init_interactive_filesystem();
void main_process_loop();
void notify_plugins(std::string trigger, std::string data);
void process_source_file();
void process_profile_file();
void process_profile_startup_args();
void apply_profile_startup_args();
void create_profile_file();
void create_source_file();
bool is_parent_process_alive();
void setup_signal_handlers();
void save_terminal_state();
void restore_terminal_state();
void setup_environment_variables();
void initialize_shell_environment();
bool is_shell_script_construct(const std::string& line);
void process_shell_scripts_in_config();
bool parse_and_set_env_var(const std::string& line);
std::string get_current_time_string();
void handle_command_error(const std::string& command,
                          const std::string& error_message);
void reprint_prompt();