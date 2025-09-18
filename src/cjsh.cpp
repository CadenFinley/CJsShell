#include "cjsh.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

#include "builtin.h"
#include "cjsh_completions.h"
#include "cjsh_filesystem.h"
#include "colors.h"
#include "error_out.h"
#include "isocline/isocline.h"
#include "job_control.h"
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

static int parse_command_line_arguments(int argc, char* argv[],
                                        std::string& script_file,
                                        std::vector<std::string>& script_args);
static int handle_early_exit_modes();
static int handle_non_interactive_mode(const std::string& script_file);
static int initialize_interactive_components();
static void detect_login_mode(char* argv[]);
static void save_startup_arguments(int argc, char* argv[]);
static void main_process_loop();
static bool init_login_filesystem();
static bool init_interactive_filesystem();
static void notify_plugins(std::string trigger, std::string data);
static void process_source_file();
static void process_profile_file();
static void apply_profile_startup_flags();
static void create_profile_file();
static void create_source_file();
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
bool show_version = false;
bool show_help = false;
bool startup_test = false;
}  // namespace config

static void initialize_title_strings() {
  if (title_line.empty()) {
    title_line = " CJ's Shell v" + c_version +
                 (PRE_RELEASE ? pre_release_line : "") +
                 " - Caden J Finley (c) 2025";
  }
  if (created_line.empty()) {
    created_line =
        " Created 2025 @ \033[1;35mAbilene Christian University\033[0m";
  }
}

// fatal: could not create work tree dir '/nvim': Read-only file system
// git clone https://github.com/nvim-lua/kickstart.nvim.git
// "${XDG_CONFIG_HOME:-$HOME/.config}"/nvim

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
  // Detect login mode from argv[0]
  detect_login_mode(argv);

  // Parse command line arguments
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

  // Handle login mode initialization
  if (config::login_mode) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing login environment" << std::endl;
    if (!init_login_filesystem()) {
      print_error({ErrorType::RUNTIME_ERROR,
                   nullptr,
                   "Failed to initialize file system",
                   {"Check file permissions", "Reinstall cjsh"}});
      return 1;
    }
    process_profile_file();
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
  if (!g_exit_flag && !config::startup_test) {
    if (g_title_line) {
      initialize_title_strings();
      std::cout << title_line << std::endl;
      std::cout << created_line << std::endl;
    }
    main_process_loop();
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

static void detect_login_mode(char* argv[]) {
  if (argv && argv[0] && argv[0][0] == '-') {
    config::login_mode = true;
    if (g_debug_mode)
      std::cerr << "DEBUG: Login mode detected from argv[0]: " << argv[0]
                << std::endl;
  }
}

static int parse_command_line_arguments(int argc, char* argv[],
                                        std::string& script_file,
                                        std::vector<std::string>& script_args) {
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
      {"no-source", no_argument, 0, 'N'},
      {"no-completions", no_argument, 0, 'O'},
      {"no-syntax-highlighting", no_argument, 0, 'S'},
      {"startup-test", no_argument, 0, 'X'},
      {0, 0, 0, 0}};
  const char* short_options =
      "+lic:vhdPTACLNOSX";  // Leading '+' enables POSIXLY_CORRECT behavior
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
      case 'X':
        config::startup_test = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Startup test mode enabled" << std::endl;
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
    std::cout << c_version << (PRE_RELEASE ? pre_release_line : "")
              << std::endl;
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

    std::ifstream file(script_file);
    // check if file exists
    if (!std::filesystem::exists(script_file)) {
      print_error({ErrorType::FILE_NOT_FOUND,
                   script_file.c_str(),
                   "Script file not found",
                   {"Check file path"}});
      return 127;
    }

    // check if file opened successfully
    if (!file.is_open()) {
      print_error({ErrorType::PERMISSION_DENIED,
                   script_file.c_str(),
                   "Failed to open script file",
                   {"Check file permissions"}});
      return 127;
    }

    std::string line;
    while (std::getline(file, line)) {
      script_content += line + "\n";
    }
    file.close();
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

static int initialize_interactive_components() {
  // Set interactive mode
  g_shell->set_interactive_mode(true);

  if (!init_interactive_filesystem()) {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 "Failed to initialize file system",
                 {"Check file permissions", "Reinstall cjsh"}});
    return 1;
  }

  g_shell->setup_interactive_handlers();

  // Initialize colors module
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing colors with enabled="
              << config::colors_enabled << std::endl;
  colors::initialize_color_support(config::colors_enabled);

  // Configure isocline colors based on color settings
  if (!config::colors_enabled) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Disabling isocline colors and resetting prompt style"
                << std::endl;
    ic_enable_color(false);
    // Override the default green prompt style with no color
    ic_style_def("ic-prompt", "");
  }

  // Initialize plugins if enabled
  if (config::plugins_enabled) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing plugin system with enabled="
                << config::plugins_enabled << std::endl;
    g_plugin = std::make_unique<Plugin>(cjsh_filesystem::g_cjsh_plugin_path,
                                        config::plugins_enabled);
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: Plugins disabled, skipping initialization"
              << std::endl;
  }

  // Initialize themes if enabled
  if (config::themes_enabled) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing theme system with enabled="
                << config::themes_enabled << std::endl;
    g_theme = std::make_unique<Theme>(cjsh_filesystem::g_cjsh_theme_path,
                                      config::themes_enabled);
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: Themes disabled, skipping initialization" << std::endl;
  }

  // Initialize AI if enabled
  if (config::ai_enabled) {
    std::string api_key = "";
    const char* env_key = getenv("OPENAI_API_KEY");
    if (env_key) {
      api_key = env_key;
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing AI with enabled=" << config::ai_enabled
                << std::endl;
    g_ai = std::make_unique<Ai>(api_key, std::string("chat"), std::string(""),
                                std::vector<std::string>{},
                                cjsh_filesystem::g_cjsh_data_path,
                                config::ai_enabled);
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: AI disabled, skipping initialization" << std::endl;
  }

  // Save the current directory before processing the source file
  std::string saved_current_dir = std::filesystem::current_path().string();
  if (g_debug_mode)
    std::cerr << "DEBUG: Saved current directory: " << saved_current_dir
              << std::endl;

  // Process the source file .cjshrc
  if (config::source_enabled) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing source file" << std::endl;
    process_source_file();
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

static void update_terminal_title() {
  if (g_debug_mode) {
    std::cout << "\033]0;" << "<<<DEBUG MODE ENABLED>>>" << "\007";
    std::cout.flush();
  }
  std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
  std::cout.flush();
}

void reprint_prompt() {
  // unused function for current implementation, useful for plugins
  if (g_debug_mode) {
    std::cerr << "DEBUG: Reprinting prompt" << std::endl;
  }

  update_terminal_title();

  std::string prompt;
  if (g_shell->get_menu_active()) {
    prompt = g_shell->get_prompt();
  } else {
    prompt = g_shell->get_ai_prompt();
  }

  if (g_theme && g_theme->uses_newline()) {
    prompt += "\n";
    prompt += g_shell->get_newline_prompt();
  }
  ic_print_prompt(prompt.c_str(), false);
}

static void main_process_loop() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Entering main process loop" << std::endl;
  notify_plugins("main_process_pre_run", "");

  initialize_completion_system();

  while (true) {
    if (g_debug_mode) {
      std::cerr << "---------------------------------------" << std::endl;
      std::cerr << "DEBUG: Starting new command input cycle" << std::endl;
    }
    notify_plugins("main_process_start", "");

    // Check and handle any pending signals before prompting for input
    g_shell->process_pending_signals();

    // Update job status and clean up finished jobs
    if (g_debug_mode)
      std::cerr << "DEBUG: Calling JobManager::update_job_status()"
                << std::endl;
    JobManager::instance().update_job_status();

    if (g_debug_mode)
      std::cerr << "DEBUG: Calling JobManager::cleanup_finished_jobs()"
                << std::endl;
    JobManager::instance().cleanup_finished_jobs();

    if (g_debug_mode)
      std::cerr << "DEBUG: Calling update_terminal_title()" << std::endl;
    update_terminal_title();

    if (g_debug_mode)
      std::cerr << "DEBUG: Generating prompt" << std::endl;

    // Ensure the prompt always starts on a clean line
    // We print a space, then carriage return to detect if we're at column 0
    std::printf(" \r");
    std::fflush(stdout);

    // gather and create the prompt
    std::string prompt;
    if (g_shell->get_menu_active()) {
      prompt = g_shell->get_prompt();
    } else {
      prompt = g_shell->get_ai_prompt();
    }
    if (g_theme && g_theme->uses_newline()) {
      prompt += "\n";
      prompt += g_shell->get_newline_prompt();
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: About to call ic_readline with prompt: '" << prompt
                << "'" << std::endl;
    char* input = ic_readline(prompt.c_str());
    if (g_debug_mode)
      std::cerr << "DEBUG: ic_readline returned" << std::endl;
    if (input != nullptr) {
      std::string command(input);
      if (g_debug_mode)
        std::cerr << "DEBUG: User input: " << command << std::endl;
      ic_free(input);
      if (!command.empty()) {
        notify_plugins("main_process_command_processed", command);
        {
          // Start timing before command execution
          g_shell->start_command_timing();

          std::string status_str;
          int exit_code = g_shell->execute(command);
          status_str = std::to_string(exit_code);

          // End timing after command execution
          g_shell->end_command_timing(exit_code);

          if (g_debug_mode)
            std::cerr << "DEBUG: Command exit status: " << status_str
                      << std::endl;
          update_completion_frequency(command);
          ic_history_add(command.c_str());
          setenv("STATUS", status_str.c_str(), 1);
        }
      } else {
        // Reset timing for empty commands to clear previous command duration
        g_shell->reset_command_timing();
      }
      if (g_exit_flag) {
        break;
      }
    } else {
      continue;
    }
    notify_plugins("main_process_end", "");
    if (g_exit_flag) {
      std::cerr << "Exiting main process loop..." << std::endl;
      break;
    }
  }
}

static void notify_plugins(std::string trigger, std::string data) {
  // notify all enabled plugins of the event with data
  if (g_plugin == nullptr) {
    if (g_debug_mode)
      std::cerr << "DEBUG: notify_plugins: plugin manager is nullptr"
                << std::endl;
    return;
  }
  if (g_plugin->get_enabled_plugins().empty()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: notify_plugins: no enabled plugins" << std::endl;
    return;
  }
  if (g_debug_mode) {
    std::cerr << "DEBUG: Notifying plugins of trigger: " << trigger
              << " with data: " << data << std::endl;
  }
  g_plugin->trigger_subscribed_global_event(trigger, data);
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

static bool init_login_filesystem() {
  // verify and create if needed the cjsh login filesystem
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing login filesystem" << std::endl;
  try {
    if (!std::filesystem::exists(cjsh_filesystem::g_user_home_path)) {
      print_error({ErrorType::RUNTIME_ERROR,
                   nullptr,
                   "User home path not found",
                   {"Check user account configuration"}});
      return false;
    }

    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating profile file" << std::endl;
      create_profile_file();
    }
  } catch (const std::exception& e) {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 "Failed to initialize login filesystem",
                 {"Check file permissions", "Reinstall cjsh"}});
    return false;
  }
  return true;
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
    std::string status_str = std::to_string(0);
    env_vars.emplace_back("STATUS", status_str.c_str());
    env_vars.emplace_back("VERSION", c_version.c_str());

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

static bool init_interactive_filesystem() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing interactive filesystem" << std::endl;

  // Cache current path to avoid multiple filesystem calls
  std::string current_path = std::filesystem::current_path().string();
  if (g_debug_mode)
    std::cerr << "DEBUG: Current path: " << current_path << std::endl;
  setenv("PWD", current_path.c_str(), 1);

  try {
    // Cache file existence results to avoid repeated checks
    bool home_exists =
        std::filesystem::exists(cjsh_filesystem::g_user_home_path);
    bool history_exists =
        std::filesystem::exists(cjsh_filesystem::g_cjsh_history_path);
    bool source_exists =
        std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path);
    bool should_refresh_cache =
        cjsh_filesystem::should_refresh_executable_cache();

    if (!home_exists) {
      print_error({ErrorType::RUNTIME_ERROR,
                   nullptr,
                   "User home path not found",
                   {"Check user account configuration"}});
      return false;
    }

    // Create files if needed based on cached existence checks
    if (!history_exists) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating history file" << std::endl;
      std::ofstream history_file(cjsh_filesystem::g_cjsh_history_path);
      history_file.close();
    }

    // .cjshrc
    if (!source_exists) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating source file" << std::endl;
      create_source_file();
    }

    // Only refresh executable cache if needed
    if (should_refresh_cache) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Refreshing executable cache" << std::endl;
      cjsh_filesystem::build_executable_cache();
    } else {
      if (g_debug_mode)
        std::cerr << "DEBUG: Using existing executable cache" << std::endl;
    }

  } catch (const std::exception& e) {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 "Failed to initialize interactive filesystem",
                 {"Check file permissions", "Reinstall cjsh"}});
    return false;
  }
  return true;
}

static void process_profile_file() {
  // sourcing if in login shell
  if (g_debug_mode)
    std::cerr << "DEBUG: Processing profile files" << std::endl;
  std::filesystem::path universal_profile = "/etc/profile";
  if (std::filesystem::exists(universal_profile)) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Found universal profile: "
                << universal_profile.string() << std::endl;
    g_shell->execute("source " + universal_profile.string());
  }
  std::filesystem::path user_profile =
      cjsh_filesystem::g_user_home_path / ".profile";
  if (std::filesystem::exists(user_profile)) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Found user profile: " << user_profile.string()
                << std::endl;
    g_shell->execute("source " + user_profile.string());
  }
  // Source the profile file normally
  if (g_debug_mode)
    std::cerr << "DEBUG: Sourcing profile file: "
              << cjsh_filesystem::g_cjsh_profile_path.string() << std::endl;
  g_shell->execute("source " + cjsh_filesystem::g_cjsh_profile_path.string());
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
    }
  }
}

static void process_source_file() {
  g_shell->execute("source " + cjsh_filesystem::g_cjsh_source_path.string());
}

static void create_profile_file() {
  std::ofstream profile_file(cjsh_filesystem::g_cjsh_profile_path);
  if (profile_file.is_open()) {
    profile_file << "# cjsh Configuration File\n";
    profile_file << "# this file is sourced when the shell starts in login "
                    "mode and is sourced after /etc/profile and ~/.profile\n";
    profile_file << "# this file supports full shell scripting including "
                    "conditional logic\n";
    profile_file << "# Use the 'login-startup-arg' builtin command to set "
                    "startup flags conditionally\n";
    profile_file << "\n";
    profile_file
        << "# Example: Conditional startup flags based on environment\n";
    profile_file << "# if test -n \"$TMUX\"; then\n";
    profile_file << "#     echo \"In tmux session, no flags required\"\n";
    profile_file << "# else\n";
    profile_file << "#     login-startup-arg --no-plugins\n";
    profile_file << "#     login-startup-arg --no-themes\n";
    profile_file << "#     login-startup-arg --no-ai\n";
    profile_file << "#     login-startup-arg --no-colors\n";
    profile_file << "#     login-startup-arg --no-titleline\n";
    profile_file << "# fi\n";
    profile_file << "\n";
    profile_file << "# Available startup flags:\n";
    profile_file << "# login-startup-arg --login         # Enable login mode\n";
    profile_file
        << "# login-startup-arg --interactive   # Force interactive mode\n";
    profile_file << "# login-startup-arg --debug         # Enable debug mode\n";
    profile_file << "# login-startup-arg --no-plugins    # Disable plugins\n";
    profile_file << "# login-startup-arg --no-themes     # Disable themes\n";
    profile_file
        << "# login-startup-arg --no-ai         # Disable AI features\n";
    profile_file
        << "# login-startup-arg --no-colors     # Disable colorized output\n";
    profile_file
        << "# login-startup-arg --no-titleline  # Disable title line\n";
    profile_file << "# login-startup-arg --no-source     # Don't source the "
                    ".cjshrc file\n";
    profile_file
        << "# login-startup-arg --startup-test  # Enable startup test mode\n";
    profile_file.close();
  } else {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 "Failed to create profile file",
                 {"Check file permissions"}});
  }
}

static void create_source_file() {
  std::ofstream source_file(cjsh_filesystem::g_cjsh_source_path);
  if (source_file.is_open()) {
    source_file << "# cjsh Source File\n";
    source_file
        << "# this file is sourced when the shell starts in interactive mode\n";
    source_file << "# this is where your aliases, theme setup, enabled "
                   "plugins will be stored by default.\n";

    source_file << "# Alias examples\n";
    source_file << "alias ll='ls -la'\n";

    source_file << "# you can change this to load any installed theme\n";
    source_file << "theme load default\n";

    source_file << "# plugin examples\n";
    source_file << "# plugin example_plugin enable\n";

    source_file << "# Uninstall function, DO NOT REMOVE THIS FUNCTION\n";
    source_file << "cjsh_uninstall() {\n";
    source_file << "    rm -rf " << cjsh_filesystem::g_cjsh_path.string()
                << "\n";
    source_file << "    echo \"Uninstalled cjsh\"\n";
    source_file << "}\n";
    source_file.close();
  } else {
    print_error({ErrorType::RUNTIME_ERROR,
                 nullptr,
                 "Failed to create source file",
                 {"Check file permissions"}});
  }
}