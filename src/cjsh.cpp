#include "cjsh.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "builtin.h"
#include "cjsh_completions.h"
#include "cjsh_filesystem.h"
#include "colors.h"
#include "error_out.h"
#include "isocline/isocline.h"
#include "job_control.h"
#include "main_loop.h"
#include "shell.h"
#include "trap_command.h"
#include "usage.h"

bool g_debug_mode = false;
bool g_title_line = true;
struct termios g_original_termios;
bool g_terminal_state_saved = false;
int g_shell_terminal = 0;
pid_t g_shell_pgid = 0;
struct termios g_shell_tmodes;
bool g_job_control_enabled = false;
bool g_exit_flag = false;
std::string g_cached_version;
std::string g_current_theme;
std::string title_line;
std::string created_line;
bool g_startup_active = true;
std::unique_ptr<Shell> g_shell = nullptr;
std::unique_ptr<Ai> g_ai = nullptr;
std::unique_ptr<Theme> g_theme = nullptr;
std::unique_ptr<Plugin> g_plugin = nullptr;
std::vector<std::string> g_startup_args;
std::vector<std::string> g_profile_startup_args;
std::chrono::steady_clock::time_point g_startup_begin_time;

static int parse_command_line_arguments(int argc, char* argv[],
                                        std::string& script_file,
                                        std::vector<std::string>& script_args);
static int handle_early_exit_modes();
static int handle_non_interactive_mode(const std::string& script_file);
static int initialize_interactive_components();
static void save_startup_arguments(int argc, char* argv[]);
static void process_profile_files();
static void apply_profile_startup_flags();
static void setup_environment_variables();

namespace config {
bool login_mode = false;
bool interactive_mode = true;
bool force_interactive = false;
bool execute_command = false;
std::string cmd_to_execute = "";
bool plugins_enabled = true;
bool themes_enabled = true;
bool ai_enabled = true;
bool colors_enabled = true;
bool source_enabled = true;
bool completions_enabled = true;
bool syntax_highlighting_enabled = true;
bool smart_cd_enabled = true;
bool show_version = false;
bool show_help = false;
bool startup_test = false;
bool minimal_mode = false;
bool disable_custom_ls = false;
bool show_startup_time = false;
}  // namespace config

static void initialize_title_strings() {
  if (title_line.empty()) {
    title_line = " CJ's Shell v" + c_version + " - Caden J Finley (c) 2025";
  }
  if (created_line.empty()) {
    created_line =
        " Created 2025 @ \033[1;35mAbilene Christian University\033[0m";
  }
}

// TODO

// What's Currently Missing
// 1. InputMonitor System - NOT IMPLEMENTED
// No InputMonitor, input_monitor, or similar classes exist
// No input monitoring threads or background capture mechanisms
// The only reference is the TODO comment in main_loop.cpp line 185

// 2. Process stdin Detection - NOT IMPLEMENTED
// No functions to detect if foreground processes are using stdin
// No uses_stdin(), is_process_reading_stdin(), or similar functionality
// No /proc/fd or lsof integration for stdin detection

// 3. Non-blocking Input Capture - NOT IMPLEMENTED
// While isocline library has some non-blocking capabilities, there's no custom input capture system
// No background input queue management
// No mechanism to capture input while foreground processes run

// 4. Terminal Mode Management - NOT IMPLEMENTED
// No TerminalModeManager class exists
// While basic termios usage exists for job control, there's no specialized mode management for input monitoring
// No saving/restoring of terminal modes for input capture

// 5. Threading Infrastructure - PARTIALLY EXISTS
// Threading is used in the AI system (ai.cpp has std::thread for loading/cancellation)
// Basic std::thread and std::mutex capabilities are available
// However, no input-related threading exists

// 6. Signal Integration for Input Forwarding - NOT IMPLEMENTED
// While extensive signal handling exists, there's no integration for starting/stopping input monitoring
// No job state change handlers for input monitoring
// Current signal system doesn't detect foreground job stdin usage

// What Currently Exists (Relevant Infrastructure)
// Available Infrastructure You Can Build On:
// Job Control System:

// JobManager with get_current_job()
// Job state tracking (running, stopped, done)
// Foreground/background job management
// Input Buffer Architecture:

// input_buffer string in main_loop.cpp
// Integration with isocline's ic_readline()
// TODO comment indicating planned integration point
// Signal Handling Framework:

// Comprehensive signal handling in signal_handler.cpp
// Job control signal integration
// Extension points for new signal handling
// Terminal Control:

// Basic termios manipulation for job control
// tcsetpgrp() for terminal foreground control
// Terminal attribute saving/restoring
// Threading Support:

// std::thread usage in AI system
// Thread synchronization primitives available
// Conclusion
// All the major components for input forwarding are completely unimplemented. You have the basic architecture skeleton (the input_buffer and TODO comment), but you'll need to implement:

// Complete input monitoring system
// Process stdin detection logic
// Non-blocking input capture
// Thread-based input queue management
// Signal integration for job state changes
// Terminal mode management for input capture
// The good news is that your shell has solid foundational infrastructure (job control, signal handling, terminal management) that you can extend to implement these features.

// add a way to change syntax highlighter via .cjshrc
// add a way to change keybindings via .cjshrc

/*
 * Exit/Return Codes:
 * 0       - Success
 * 1       - General errors/Catchall
 * 2       - Misuse of shell builtins or syntax error
 * 126     - Command invoked cannot execute (permission problem
 * or not executable)
 * 127     - Command not found
 * 128     - Invalid argument to exit
 * 128+n   - Fatal error signal "n" (e.g., 130 = 128 + SIGINT(2) = Control-C)
 * 130     - Script terminated by Control-C (SIGINT)
 * 137     - Process killed (SIGKILL)
 * 139     - Process terminated (SIGQUIT)
 * 143     - Process terminated (SIGTERM)
 * 255     - Exit status out of range
 */

int main(int argc, char* argv[]) {
  // Start timing the startup process
  g_startup_begin_time = std::chrono::steady_clock::now();

  // Parse command line arguments (includes login mode detection)
  std::string script_file;
  std::vector<std::string> script_args;
  int parse_result =
      parse_command_line_arguments(argc, argv, script_file, script_args);
  if (parse_result != 0) {
    return parse_result;
  }

  // Initialize directories and register cleanup
  cjsh_filesystem::initialize_cjsh_directories();
  std::atexit(cleanup_resources);

  // Initialize core shell components
  g_shell = std::make_unique<Shell>();

  // Set positional parameters if we have script arguments
  if (!script_args.empty()) {
    g_shell->set_positional_parameters(script_args);
  }

  setup_environment_variables();
  save_startup_arguments(argc, argv);

  // Sync shell's environment cache from system environment
  if (g_shell) {
    g_shell->sync_env_vars_from_system();
  }

  // Handle login mode initialization
  if (config::login_mode) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing login environment" << std::endl;
    if (!cjsh_filesystem::init_login_filesystem()) {
      print_error({ErrorType::RUNTIME_ERROR,
                   nullptr,
                   "Failed to initialize file system",
                   {"Check file permissions", "Reinstall cjsh"}});
      return 1;
    }
    process_profile_files();
    apply_profile_startup_flags();
  }

  // Set shell environment variable
  if (argv[0]) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Setting $0=" << argv[0] << std::endl;
    setenv("0", argv[0], 1);
  } else {
    if (g_debug_mode)
      std::cerr << "DEBUG: Setting $0=unknown" << std::endl;
    setenv("0", "cjsh", 1);
  }

  // Handle early exit modes (version, help, command execution)
  int early_exit_result = handle_early_exit_modes();
  if (early_exit_result >= 0) {
    return early_exit_result;
  }

  // Handle non-interactive mode (script files or stdin)
  if (!config::interactive_mode && !config::force_interactive) {
    return handle_non_interactive_mode(script_file);
  }

  // Initialize interactive mode
  int interactive_result = initialize_interactive_components();
  if (interactive_result != 0) {
    return interactive_result;
  }

  // Enter main process loop
  g_startup_active = false;
  if (!g_exit_flag) {
    // Calculate startup time
    auto startup_end_time = std::chrono::steady_clock::now();
    auto startup_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            startup_end_time - g_startup_begin_time);

    // Set the startup duration as the initial command duration for the prompt
    if (g_shell && g_theme) {
      g_shell->set_initial_duration(startup_duration.count());
    }

    if (g_title_line) {
      initialize_title_strings();
      std::cout << title_line << std::endl;
      std::cout << created_line << std::endl;
    }

    if (g_title_line && config::show_startup_time) {
      std::cout << std::endl;
    }

    if (config::show_startup_time) {
      std::cout << " Started in " << startup_duration.count() << "ms."
                << std::endl;
    }

    if (!config::startup_test) {
      main_process_loop();
    }
  }

  std::cerr << "Cleaning up resources." << std::endl;

  // Check for exit code set by exit command
  const char* exit_code_str = getenv("EXIT_CODE");
  int exit_code = 0;
  if (exit_code_str) {
    exit_code = std::atoi(exit_code_str);
    unsetenv("EXIT_CODE");
  }

  return exit_code;
}

static int parse_command_line_arguments(int argc, char* argv[],
                                        std::string& script_file,
                                        std::vector<std::string>& script_args) {
  // Check if invoked as login shell (e.g., -cjsh)
  if (argv && argv[0] && argv[0][0] == '-') {
    config::login_mode = true;
    if (g_debug_mode)
      std::cerr << "DEBUG: Login mode detected from argv[0]: " << argv[0]
                << std::endl;
  }

  static struct option long_options[] = {
      {"login", no_argument, 0, 'l'},
      {"interactive", no_argument, 0, 'i'},
      {"debug", no_argument, 0, 'd'},
      {"command", required_argument, 0, 'c'},
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {"no-plugins", no_argument, 0, 'P'},
      {"no-themes", no_argument, 0, 'T'},
      {"no-ai", no_argument, 0, 'A'},
      {"no-colors", no_argument, 0, 'C'},
      {"no-titleline", no_argument, 0, 'L'},
      {"show-startup-time", no_argument, 0, 'U'},
      {"no-source", no_argument, 0, 'N'},
      {"no-completions", no_argument, 0, 'O'},
      {"no-syntax-highlighting", no_argument, 0, 'S'},
      {"no-smart-cd", no_argument, 0, 'M'},
      {"startup-test", no_argument, 0, 'X'},
      {"minimal", no_argument, 0, 'm'},
      {"disable-custom-ls", no_argument, 0, 'D'},
      {0, 0, 0, 0}};
  const char* short_options =
      "+lic:vhdPTACLUNOSMXmD";  // Leading '+' enables POSIXLY_CORRECT behavior
  int option_index = 0;
  int c;
  optind = 1;

  while ((c = getopt_long(argc, argv, short_options, long_options,
                          &option_index)) != -1) {
    switch (c) {
      case 'l':
        config::login_mode = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Login mode enabled" << std::endl;
        break;
      case 'i':
        config::force_interactive = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Interactive mode forced" << std::endl;
        break;
      case 'd':
        g_debug_mode = true;
        std::cerr << "DEBUG: Debug mode enabled" << std::endl;
        break;
      case 'c':
        config::execute_command = true;
        config::cmd_to_execute = optarg;
        config::interactive_mode = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Command to execute: " << config::cmd_to_execute
                    << std::endl;
        break;
      case 'v':
        config::show_version = true;
        config::interactive_mode = false;
        break;
      case 'h':
        config::show_help = true;
        config::interactive_mode = false;
        break;
      case 'P':
        config::plugins_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Plugins disabled" << std::endl;
        break;
      case 'T':
        config::themes_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Themes disabled" << std::endl;
        break;
      case 'A':
        config::ai_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: AI disabled" << std::endl;
        break;
      case 'C':
        config::colors_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Colors disabled" << std::endl;
        break;
      case 'L':
        g_title_line = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Title line disabled" << std::endl;
        break;
      case 'U':
        config::show_startup_time = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Startup time display enabled" << std::endl;
        break;
      case 'N':
        config::source_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Source file disabled" << std::endl;
        break;
      case 'O':
        config::completions_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Completions disabled" << std::endl;
        break;
      case 'S':
        config::syntax_highlighting_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Syntax highlighting disabled" << std::endl;
        break;
      case 'M':
        config::smart_cd_enabled = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Smart cd disabled" << std::endl;
        break;
      case 'X':
        config::startup_test = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Startup test mode enabled" << std::endl;
        break;
      case 'm':
        config::minimal_mode = true;
        config::plugins_enabled = false;
        config::themes_enabled = false;
        config::ai_enabled = false;
        config::colors_enabled = false;
        config::source_enabled = false;
        config::completions_enabled = false;
        config::syntax_highlighting_enabled = false;
        config::smart_cd_enabled = false;
        config::disable_custom_ls = true;
        config::show_startup_time = false;
        g_title_line = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Minimal mode enabled - all features disabled"
                    << std::endl;
        break;
      case 'D':
        config::disable_custom_ls = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Disable custom ls enabled" << std::endl;
        break;
      case '?':
        print_usage();
        return 127;
      default:
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::string(1, c),
                     "Unrecognized option",
                     {"Check command line arguments"}});
        return 127;
    }
  }

  // Check if there are script files to execute
  if (optind < argc) {
    script_file = argv[optind];
    config::interactive_mode = false;
    if (g_debug_mode)
      std::cerr << "DEBUG: Script file specified: " << script_file << std::endl;

    // Collect script arguments (everything after the script file name)
    for (int i = optind + 1; i < argc; i++) {
      script_args.push_back(argv[i]);
      if (g_debug_mode)
        std::cerr << "DEBUG: Script argument " << (i - optind) << ": "
                  << argv[i] << std::endl;
    }
  }

  // Check if stdin is a terminal - if not, disable interactive mode
  if (!config::force_interactive && !isatty(STDIN_FILENO)) {
    config::interactive_mode = false;
    if (g_debug_mode)
      std::cerr << "DEBUG: Disabling interactive mode (stdin is not a terminal)"
                << std::endl;
  }

  return 0;
}

static void save_startup_arguments(int argc, char* argv[]) {
  g_startup_args.clear();
  for (int i = 0; i < argc; i++) {
    g_startup_args.push_back(std::string(argv[i]));
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Starting CJ's Shell version " << c_version
              << " with PID: " << getpid()
              << " with original command line args: " << std::endl;
    for (const auto& arg : g_startup_args) {
      std::cerr << "DEBUG:   " << arg << std::endl;
    }
  }
}

static int handle_early_exit_modes() {
  if (config::show_version) {  // -v --version
    std::cout << c_version << std::endl;
    return 0;
  }

  if (config::show_help) {  // -h --help
    print_usage();
    return 0;
  }

  if (config::execute_command) {  // -c --command
    if (g_debug_mode) {
      std::cerr << "DEBUG: Executing -c via Shell::execute: "
                << config::cmd_to_execute << std::endl;
    }

    // Set shell to non-interactive mode for command execution
    if (g_shell) {
      g_shell->set_interactive_mode(false);
    }

    int code = g_shell ? g_shell->execute(config::cmd_to_execute) : 1;

    // Check if an exit code was set by the exit command
    const char* exit_code_str = getenv("EXIT_CODE");
    if (exit_code_str) {
      code = std::atoi(exit_code_str);
      unsetenv("EXIT_CODE");
    }

    // Execute EXIT trap before resetting shell for -c commands
    if (g_shell) {
      TrapManager::instance().set_shell(g_shell.get());
      TrapManager::instance().execute_exit_trap();
    }

    return code;
  }

  return -1;  // Continue execution
}

static int handle_non_interactive_mode(const std::string& script_file) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Running in non-interactive mode" << std::endl;

  std::string script_content;

  // If a script file was specified, read it
  if (!script_file.empty()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Reading script file: " << script_file << std::endl;

    auto read_result =
        cjsh_filesystem::FileOperations::read_file_content(script_file);
    if (!read_result.is_ok()) {
      // Determine appropriate error type based on the error message
      ErrorType error_type = ErrorType::FILE_NOT_FOUND;
      if (read_result.error().find("Permission denied") != std::string::npos) {
        error_type = ErrorType::PERMISSION_DENIED;
      }

      print_error({error_type,
                   script_file.c_str(),
                   read_result.error().c_str(),
                   {"Check file path and permissions"}});
      return 127;
    }

    script_content = read_result.value();
  } else {
    // Read and execute input from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
      script_content += line + "\n";
    }
  }

  if (!script_content.empty()) {
    if (g_debug_mode) {
      if (!script_file.empty()) {
        std::cerr << "DEBUG: Executing script file content" << std::endl;
      } else {
        std::cerr << "DEBUG: Executing piped script content" << std::endl;
      }
    }
    int code = g_shell ? g_shell->execute(script_content) : 1;

    // Check if an exit code was set by the exit command
    const char* exit_code_str = getenv("EXIT_CODE");
    if (exit_code_str) {
      code = std::atoi(exit_code_str);
      unsetenv("EXIT_CODE");
    }

    return code;
  }

  return 0;
}

void initialize_colors() {
  // Initialize colors module
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing colors with enabled="
              << config::colors_enabled << std::endl;
  colors::initialize_color_support(config::colors_enabled);
  if (g_debug_mode)
    std::cerr << "DEBUG: Disabling isocline colors and resetting prompt style"
              << std::endl;

  if (!config::colors_enabled) {
    ic_enable_color(false);
    // Override the default green prompt style with no color
    ic_style_def("ic-prompt", "");
    if (g_debug_mode)
      std::cerr << "DEBUG: Colors disabled." << std::endl;
  }
}

void initialize_plugins() {
  // Initialize plugins if enabled
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing plugin system with enabled="
              << config::plugins_enabled << std::endl;
  g_plugin = std::make_unique<Plugin>(cjsh_filesystem::g_cjsh_plugin_path,
                                      config::plugins_enabled, true);
}

void initialize_themes() {
  // Initialize themes if enabled
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing theme system with enabled="
              << config::themes_enabled << std::endl;
  g_theme = std::make_unique<Theme>(cjsh_filesystem::g_cjsh_theme_path,
                                    config::themes_enabled);
}

void initialize_ai() {
  // Initialize AI if enabled
  std::string api_key = "";
  const char* env_key = getenv("OPENAI_API_KEY");
  if (env_key) {
    api_key = env_key;
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing AI with enabled=" << config::ai_enabled
              << std::endl;
  g_ai = std::make_unique<Ai>(
      api_key, std::string("chat"), std::string(""), std::vector<std::string>{},
      cjsh_filesystem::g_cjsh_data_path, config::ai_enabled);
}

static int initialize_interactive_components() {
  // Set interactive mode
  g_shell->set_interactive_mode(true);

  if (!cjsh_filesystem::init_interactive_filesystem()) {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 "Failed to initialize file system",
                 {"Check file permissions", "Reinstall cjsh"}});
    return 1;
  }

  g_shell->setup_interactive_handlers();

  // colors is the only component that must be initialized before
  // plugins/themes/ai as they depend on it themes plugins and ai are lazy
  // loaded
  initialize_colors();

  // Save the current directory before processing the source file
  std::string saved_current_dir = std::filesystem::current_path().string();
  if (g_debug_mode)
    std::cerr << "DEBUG: Saved current directory: " << saved_current_dir
              << std::endl;

  // Process the source file .cjshrc
  if (config::source_enabled) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing source file" << std::endl;
    if (cjsh_filesystem::file_exists(cjsh_filesystem::g_cjsh_source_path)) {
      g_shell->execute_script_file(cjsh_filesystem::g_cjsh_source_path);
    }
  } else {
    if (g_debug_mode)
      std::cerr << "DEBUG: Restoring current directory due to --no-source: "
                << saved_current_dir << std::endl;
    if (std::filesystem::current_path() != saved_current_dir) {
      std::filesystem::current_path(saved_current_dir);
      setenv("PWD", saved_current_dir.c_str(), 1);
      g_shell->get_built_ins()->set_current_directory();
    }
  }

  return 0;
}

void cleanup_resources() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: Cleaning up resources..." << std::endl;
  }

  // Execute EXIT trap before cleanup (while shell is still available)
  if (g_shell) {
    TrapManager::instance().set_shell(g_shell.get());
    TrapManager::instance().execute_exit_trap();
  }

  // Only cleanup AI if it was initialized
  if (g_ai) {
    g_ai.reset();
  }

  // Only cleanup Theme if it was initialized
  if (g_theme) {
    g_theme.reset();
  }

  // Only cleanup Plugin if it was initialized
  if (g_plugin) {
    g_plugin.reset();
  }

  // Reset the shell last - its destructor will handle process cleanup
  if (g_shell) {
    g_shell.reset();
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Cleanup complete." << std::endl;
  }
  if (config::interactive_mode) {
    std::cerr << "Shutdown complete." << std::endl;
  }
}

static void setup_environment_variables() {
  // setup essential environment variables for the shell session
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting up environment variables" << std::endl;

  uid_t uid = getuid();
  struct passwd* pw = getpwuid(uid);

  if (pw != nullptr) {
    // Prepare all environment variables in a batch
    std::vector<std::pair<const char*, const char*>> env_vars;

    // User info variables
    env_vars.emplace_back("USER", pw->pw_name);
    env_vars.emplace_back("LOGNAME", pw->pw_name);
    env_vars.emplace_back("HOME", pw->pw_dir);

    // Check PATH and add if needed
    const char* path_env = getenv("PATH");
    if (!path_env || path_env[0] == '\0') {
      env_vars.emplace_back("PATH",
                            "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin");
    }

    // System info
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      env_vars.emplace_back("HOSTNAME", hostname);
    }

    // Current directory and shell info
    std::string current_path = std::filesystem::current_path().string();
    std::string shell_path = cjsh_filesystem::get_cjsh_path().string();

    if (g_debug_mode) {
      std::cerr << "DEBUG: Setting SHELL to: " << shell_path << std::endl;
    }

    env_vars.emplace_back("PWD", current_path.c_str());
    env_vars.emplace_back("SHELL", shell_path.c_str());
    env_vars.emplace_back("IFS", " \t\n");

    // Language settings
    const char* lang_env = getenv("LANG");
    if (!lang_env || lang_env[0] == '\0') {
      env_vars.emplace_back("LANG", "en_US.UTF-8");
    }

    // Optional variables
    if (getenv("PAGER") == nullptr) {
      env_vars.emplace_back("PAGER", "less");
    }

    if (getenv("TMPDIR") == nullptr) {
      env_vars.emplace_back("TMPDIR", "/tmp");
    }

    // Shell level
    int shlvl = 1;
    if (const char* current_shlvl = getenv("SHLVL")) {
      try {
        shlvl = std::stoi(current_shlvl) + 1;
      } catch (...) {
        shlvl = 1;
      }
    }
    std::string shlvl_str = std::to_string(shlvl);
    env_vars.emplace_back("SHLVL", shlvl_str.c_str());

    // Miscellaneous
    std::string cjsh_path = cjsh_filesystem::get_cjsh_path().string();
    if (g_debug_mode) {
      std::cerr << "DEBUG: Setting _ to: " << cjsh_path << std::endl;
    }
    env_vars.emplace_back("_", cjsh_path.c_str());
    // Use bash-like exit status variable instead of STATUS
    std::string status_str = std::to_string(0);
    env_vars.emplace_back("?", status_str.c_str());
    // Set shell-specific version variable (optional)
    env_vars.emplace_back("CJSH_VERSION", c_version.c_str());

    // Set all environment variables in one batch
    if (g_debug_mode) {
      std::cerr << "DEBUG: Setting " << env_vars.size()
                << " environment variables" << std::endl;
    }

    // Actually set the environment variables
    for (const auto& [name, value] : env_vars) {
      setenv(name, value, 1);
    }
  }
}

static void process_profile_files() {
  // sourcing if in login shell
  if (g_debug_mode)
    std::cerr << "DEBUG: Processing profile files" << std::endl;
  std::filesystem::path universal_profile = "/etc/profile";
  if (std::filesystem::exists(universal_profile)) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Found universal profile: "
                << universal_profile.string() << std::endl;
    g_shell->execute_script_file(universal_profile, true);
  }
  std::filesystem::path user_profile =
      cjsh_filesystem::g_user_home_path / ".profile";
  if (std::filesystem::exists(user_profile)) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Found user profile: " << user_profile.string()
                << std::endl;
    g_shell->execute_script_file(user_profile, true);
  }
  // Source the profile file normally
  if (g_debug_mode)
    std::cerr << "DEBUG: Sourcing profile file: "
              << cjsh_filesystem::g_cjsh_profile_path.string() << std::endl;
  g_shell->execute_script_file(cjsh_filesystem::g_cjsh_profile_path);
}

static void apply_profile_startup_flags() {
  // Apply startup flags that were collected during profile processing
  if (g_debug_mode) {
    std::cerr << "DEBUG: Applying profile startup flags" << std::endl;
    if (g_profile_startup_args.empty()) {
      std::cerr << "DEBUG: No profile startup flags to process" << std::endl;
    } else {
      std::cerr << "DEBUG: Profile startup flags to process:" << std::endl;
      for (const auto& flag : g_profile_startup_args) {
        std::cerr << "DEBUG:   " << flag << std::endl;
      }
    }
  }

  for (const std::string& flag : g_profile_startup_args) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing profile startup flag: " << flag
                << std::endl;

    if (flag == "--debug") {
      g_debug_mode = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Debug mode enabled via profile" << std::endl;
    } else if (flag == "--no-plugins") {
      config::plugins_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Plugins disabled via profile" << std::endl;
    } else if (flag == "--no-themes") {
      config::themes_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Themes disabled via profile" << std::endl;
    } else if (flag == "--no-ai") {
      config::ai_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: AI disabled via profile" << std::endl;
    } else if (flag == "--no-colors") {
      config::colors_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Colors disabled via profile" << std::endl;
    } else if (flag == "--no-titleline") {
      g_title_line = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Title line disabled via profile" << std::endl;
    } else if (flag == "--show-startup-time") {
      config::show_startup_time = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Startup time display enabled via profile"
                  << std::endl;
    } else if (flag == "--no-source") {
      config::source_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Source file disabled via profile" << std::endl;
    } else if (flag == "--no-completions") {
      config::completions_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Completions disabled via profile" << std::endl;
    } else if (flag == "--no-syntax-highlighting") {
      config::syntax_highlighting_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Syntax highlighting disabled via profile"
                  << std::endl;
    } else if (flag == "--no-smart-cd") {
      config::smart_cd_enabled = false;
      if (g_debug_mode)
        std::cerr << "DEBUG: Smart cd disabled via profile" << std::endl;
    } else if (flag == "--startup-test") {
      config::startup_test = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Startup test mode enabled via profile"
                  << std::endl;
    } else if (flag == "--interactive") {
      config::force_interactive = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Interactive mode forced via profile" << std::endl;
    } else if (flag == "--login") {
      // Login mode is already set during initial argument processing
      if (g_debug_mode)
        std::cerr
            << "DEBUG: Login mode flag found in profile (already processed)"
            << std::endl;
    } else if (flag == "--minimal") {
      config::minimal_mode = true;
      config::plugins_enabled = false;
      config::themes_enabled = false;
      config::ai_enabled = false;
      config::colors_enabled = false;
      config::source_enabled = false;
      config::completions_enabled = false;
      config::syntax_highlighting_enabled = false;
      config::smart_cd_enabled = false;
      config::disable_custom_ls = true;
      config::show_startup_time = false;
      g_title_line = false;
      if (g_debug_mode)
        std::cerr
            << "DEBUG: Minimal mode enabled via profile - all features disabled"
            << std::endl;
    } else if (flag == "--disable-custom-ls") {
      config::disable_custom_ls = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Disable custom ls enabled via profile"
                  << std::endl;
    }
  }
}
