#include "ai.h"
#include "shell.h"
#include "theme.h"
#include "plugin.h"
#include "../isocline/include/isocline.h"
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>

// constants
const std::string c_version = "2.0.2.4";
const std::string c_github_url = "https://github.com/CadenFinley/CJsShell";
const std::string c_update_url = "https://api.github.com/repos/cadenfinley/CJsShell/releases/latest";
const pid_t c_pid = std::to_string(getpid());

// constant colors
const std::string c_reset_color = "\033[0m";
const std::string c_title_color = "\033[1;35m";

// globals
bool g_debug_mode = false;
bool g_silent_startup = false;
bool g_cached_update = false;
bool g_source = true;
bool g_check_updates = true;
bool g_title_line = true;
bool g_menu_terminal = true;
struct termios g_original_termios;
bool g_terminal_state_saved = false;
int g_shell_terminal;
pid_t g_shell_pgid = 0;
struct termios g_shell_tmodes;
bool g_job_control_enabled = false;

time_t g_last_update_check = 0;
int g_update_check_interval = 86400; // 24 hours
std::string g_cached_version = "";
std::string g_last_updated = "";

std::vector<std::string> g_startup_commands;

// the cjsh file system
struct cjsh_filesystem {
  // ALL STORED IN FULL PATHS
  std::string g_user_home_path = std::getenv("HOME");
  std::string g_cjsh_path = ""; //this will be determined at runtime

  // used if login
  std::string g_cjsh_config_path = g_user_home_path + "/.cjprofile"; //envvars and PATH setup

  // used if interactive
  std::string g_cjsh_data_path = g_user_home_path + "/.cjsh_data";
  std::string g_cjsh_plugin_path = g_cjsh_data_path + "/plugins";
  std::string g_cjsh_theme_path = g_cjsh_data_path + "/themes";
  std::string g_cjsh_history_path = g_cjsh_data_path + "/history.txt";
  std::string g_cjsh_uninstall_path = g_cjsh_data_path + "/uninstall.sh";
  std::string g_cjsh_update_cache_path = g_cjsh_data_path + "/update_cache.json";
  std::string g_cjsh_source_path = g_user_home_path + "/.cjshrc"; // aliases, prompt, functions, themes
};


// theme name the theme manger will load this
std::string g_current_theme = "default";

// misc
std::string g_shortcut_prefix = "@";
std::string title_line = "CJ's Shell v" + current_version + " - Caden J Finley (c) 2025";
std::string created_line = "Created 2025 @ " + PURPLE_COLOR_BOLD + "Abilene Christian University" + RESET_COLOR;

// objects
AI* g_ai = nullptr;
Shell* g_shell = nullptr;
Theme* g_theme = nullptr;
Plugin* g_plugin = nullptr;


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
static void signal_handler_wrapper(int signum, siginfo_t* info, void* context);
void save_terminal_state();
void restore_terminal_state();
void setup_job_control();
void setup_environment_variables();
void initialize_login_environment();