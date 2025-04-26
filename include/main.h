#pragma once

#include "ai.h"
#include "shell.h"
#include "theme.h"
#include "plugin.h"
#include "cjsh_filesystem.h"
#include "../isocline/include/isocline.h"
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

// constants
const std::string c_version = "2.0.2.4";
const std::string c_github_url = "https://github.com/CadenFinley/CJsShell";
const std::string c_update_url = "https://api.github.com/repos/cadenfinley/CJsShell/releases/latest";
const pid_t c_pid = getpid();  // Fixed: removed std::to_string
const std::string c_pid_str = std::to_string(getpid());  // String version if needed

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
bool g_silent_update_check = true;
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

// Global reference to the installation path
std::string g_cjsh_path = "";
std::string g_user_home_path = std::getenv("HOME") ? std::getenv("HOME") : "";

// theme name the theme manger will load this
std::string g_current_theme = "default";

// misc
std::string g_shortcut_prefix = "@";
std::string title_line = "CJ's Shell v" + c_version + " - Caden J Finley (c) 2025";
std::string created_line = "Created 2025 @ " + c_title_color + "Abilene Christian University" + c_reset_color;

// objects
Ai* g_ai = nullptr;
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
void save_terminal_state();
void restore_terminal_state();
void setup_job_control();
void setup_environment_variables();
void initialize_login_environment();

using json = nlohmann::json;

bool check_for_update();
bool load_update_cache();
void save_update_cache(bool update_available, const std::string& latest_version);
bool execute_update_if_available(bool update_available);
bool should_check_for_updates();
bool download_latest_release();
void display_changelog(const std::string& changelog_path);
std::string get_current_time_string();
bool is_newer_version(const std::string& latest, const std::string& current);
