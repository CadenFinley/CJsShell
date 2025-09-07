#include "cjsh.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

#include "builtin.h"
#include "cjsh_filesystem.h"
#include "colors.h"
#include "completions.h"
#include "isocline/isocline.h"
#include "shell.h"
#include "update.h"
#include "usage.h"

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
bool show_version = false;
bool show_help = false;
bool check_update = false;
bool startup_test = false;
}  // namespace config

// to do
//  local session history files, that combine into main one upon process close
//  rework ai system to always retrieve api key from envvar
//  rework builtins to be more POSIX compliant syntax wise
//  remove all system() calls in cjsh

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
  // Check if started as a login shell -cjsh
  if (argv && argv[0] && argv[0][0] == '-') {
    config::login_mode = true;
    if (g_debug_mode)
      std::cerr << "DEBUG: Login mode detected from argv[0]: " << argv[0]
                << std::endl;
  }

  // Setup long options
  static struct option long_options[] = {
      {"login", no_argument, 0, 'l'},
      {"interactive", no_argument, 0, 'i'},
      {"debug", no_argument, 0, 'd'},
      {"command", required_argument, 0, 'c'},
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {"update", no_argument, 0, 'u'},
      {"silent-updates", no_argument, 0, 'S'},
      {"no-plugins", no_argument, 0, 'P'},
      {"no-themes", no_argument, 0, 'T'},
      {"no-ai", no_argument, 0, 'A'},
      {"no-colors", no_argument, 0, 'C'},
      {"no-update", no_argument, 0, 'U'},
      {"check-update", no_argument, 0, 'V'},
      {"no-titleline", no_argument, 0, 'L'},
      {"no-source", no_argument, 0, 'N'},
      {"startup-test", no_argument, 0, 'X'},
      {0, 0, 0, 0}};
  const char* short_options = "lic:vhduSPTACUVLNX";
  int option_index = 0;
  int c;
  optind = 1;

  // First, process any profile startup args to ensure they are included
  while ((c = getopt_long(argc, argv, short_options, long_options,
                          &option_index)) != -1) {
    switch (c) {
      case 'l':
        config::login_mode = true;
        if (g_debug_mode) std::cerr << "DEBUG: Login mode enabled" << std::endl;
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
      case 'u':
        config::check_update = true;
        config::interactive_mode = false;
        break;
      case 'S':
        g_silent_update_check = true;
        break;
      case 'P':
        config::plugins_enabled = false;
        if (g_debug_mode) std::cerr << "DEBUG: Plugins disabled" << std::endl;
        break;
      case 'T':
        config::themes_enabled = false;
        if (g_debug_mode) std::cerr << "DEBUG: Themes disabled" << std::endl;
        break;
      case 'A':
        config::ai_enabled = false;
        if (g_debug_mode) std::cerr << "DEBUG: AI disabled" << std::endl;
        break;
      case 'C':
        config::colors_enabled = false;
        if (g_debug_mode) std::cerr << "DEBUG: Colors disabled" << std::endl;
        break;
      case 'U':
        g_check_updates = false;
        if (g_debug_mode)
          std::cerr << "DEBUG: Update checks disabled" << std::endl;
        break;
      case 'V':
        g_check_updates = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Update checks enabled" << std::endl;
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
      case 'X':
        config::startup_test = true;
        if (g_debug_mode)
          std::cerr << "DEBUG: Startup test mode enabled" << std::endl;
        break;
      case '?':
        print_usage();
        return 127;
      default:
        std::cerr << "Unexpected error in argument parsing." << std::endl;
        return 127;
    }
  }

  // create the shell component
  g_shell = std::make_unique<Shell>(config::login_mode);

  // Check if stdin is a terminal - if not, disable interactive mode
  if (!config::force_interactive && !isatty(STDIN_FILENO)) {
    config::interactive_mode = false;
    if (g_debug_mode)
      std::cerr << "DEBUG: Disabling interactive mode (stdin is not a terminal)"
                << std::endl;
  }

  // load startup args to save for restarts
  g_startup_args.clear();
  for (int i = 0; i < argc; i++) {
    g_startup_args.push_back(std::string(argv[i]));
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Starting CJ's Shell version " << c_version
              << " with PID: " << c_pid_str
              << " with command line args: " << std::endl;
    for (const auto& arg : g_startup_args) {
      std::cerr << "DEBUG:   " << arg << std::endl;
    }
  }

  // create the cjsh login environment if needed
  if (g_shell->get_login_mode()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing login environment" << std::endl;
    if (!init_login_filesystem()) {
      std::cerr << "Error: Failed to initialize or verify file system or files "
                   "within the file system."
                << std::endl;
      g_shell.reset();
      return 1;
    }
    // create cjsh login environment
    process_profile_file();
  }

  // create the shell environment
  initialize_shell_environment();
  setup_environment_variables();

  // set env vars to reflect cjsh being the shell
  if (argv[0]) {
    if (g_debug_mode) std::cerr << "DEBUG: Setting $0=" << argv[0] << std::endl;
    setenv("0", argv[0], 1);
  } else {
    if (g_debug_mode) std::cerr << "DEBUG: Setting $0=unknown" << std::endl;
    setenv("0", "cjsh", 1);
  }

  // start processing simple flags
  if (config::show_version) {  // -v --version
    std::cout << c_version << (PRE_RELEASE ? "-PRERELEASE" : "") << std::endl;
  } else if (config::show_help) {  // -h --help
    print_usage();
  } else if (config::check_update) {  // -u --update
    execute_update_if_available(check_for_update());
  } else if (config::execute_command) {  // -c --command
    if (g_debug_mode) {
      std::cerr << "DEBUG: Executing -c via Shell::execute: "
                << config::cmd_to_execute << std::endl;
    }

    // The preprocessing is now handled in the parser layer
    int code = g_shell ? g_shell->execute(config::cmd_to_execute) : 1;
    g_shell.reset();
    return code;
  }

  if (!config::interactive_mode && !config::force_interactive) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Running in non-interactive mode" << std::endl;

    // Read and execute input from stdin
    std::string line;
    std::string script_content;
    while (std::getline(std::cin, line)) {
      script_content += line + "\n";
    }

    if (!script_content.empty()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Executing piped script content" << std::endl;
      int code = g_shell ? g_shell->execute(script_content) : 1;
      g_shell.reset();
      return code;
    }

    g_shell.reset();
    return 0;
  }

  // at this point we have not called any flags or caused any issues to make the
  // shell close and exit making it non interactive so past this point we are in
  // interactive mode
  g_shell->set_interactive_mode(true);
  if (!init_interactive_filesystem()) {
    std::cerr << "Error: Failed to initialize or verify file system or files "
                 "within the file system."
              << std::endl;
    g_shell.reset();
    return 1;
  }
  g_shell->setup_interactive_handlers();

  // create colors module
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing colors with enabled="
              << config::colors_enabled << std::endl;
  colors::initialize_color_support(config::colors_enabled);

  // Only create Plugin object if plugins are enabled
  std::unique_ptr<Plugin> plugin;
  if (config::plugins_enabled) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing plugin system with enabled="
                << config::plugins_enabled << std::endl;
    plugin = std::make_unique<Plugin>(cjsh_filesystem::g_cjsh_plugin_path,
                                      config::plugins_enabled);
    g_plugin = plugin.get();
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: Plugins disabled, skipping initialization"
              << std::endl;
  }

  // Only create Theme object if themes are enabled
  std::unique_ptr<Theme> theme;
  if (config::themes_enabled) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing theme system with enabled="
                << config::themes_enabled << std::endl;
    theme = std::make_unique<Theme>(cjsh_filesystem::g_cjsh_theme_path,
                                    config::themes_enabled);
    g_theme = theme.get();
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: Themes disabled, skipping initialization" << std::endl;
  }

  // Only create Ai object if AI is enabled
  std::unique_ptr<Ai> ai;
  if (config::ai_enabled) {
    std::string api_key = "";
    const char* env_key = getenv("OPENAI_API_KEY");
    if (env_key) {
      api_key = env_key;
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: Initializing AI with enabled=" << config::ai_enabled
                << std::endl;
    ai = std::make_unique<Ai>(api_key, std::string("chat"), std::string(""),
                              std::vector<std::string>{},
                              cjsh_filesystem::g_cjsh_data_path,
                              config::ai_enabled);
    g_ai = ai.get();
  } else if (g_debug_mode) {
    std::cerr << "DEBUG: AI disabled, skipping initialization" << std::endl;
  }

  // Save the current directory before processing the source file
  std::string saved_current_dir = std::filesystem::current_path().string();
  if (g_debug_mode)
    std::cerr << "DEBUG: Saved current directory: " << saved_current_dir
              << std::endl;

  // process the source file .cjshrc
  if (config::source_enabled) {
    if (g_debug_mode) std::cerr << "DEBUG: Processing source file" << std::endl;
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

  // startup is complete and we enter the main process loop
  g_startup_active = false;
  if (!g_exit_flag && !config::startup_test) {
    startup_update_process();  // check for updates after startup is complete
    if (g_title_line) {
      std::cout << title_line << std::endl;
      std::cout << created_line << std::endl;
    }

    main_process_loop();
  }

  std::cout << "Cleaning up resources..." << std::endl;

  // Only cleanup AI if it was initialized
  if (ai) {
    g_ai = nullptr;
    ai.reset();
  }

  // Only cleanup Theme if it was initialized
  if (theme) {
    g_theme = nullptr;
    theme.reset();
  }

  // Only cleanup Plugin if it was initialized
  if (plugin) {
    g_plugin = nullptr;
    plugin.reset();
  }

  g_shell.reset();

  std::cout << "Cleanup complete." << std::endl;
  return 0;
}

void update_terminal_title() {
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
    std::cout << prompt << std::endl;
    prompt = g_shell->get_newline_prompt();
  }
  ic_print_prompt(prompt.c_str(), false);
}

void main_process_loop() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Entering main process loop" << std::endl;
  notify_plugins("main_process_pre_run", c_pid_str);

  initialize_completion_system();

  while (true) {
    if (g_debug_mode) {
      std::cerr << "---------------------------------------" << std::endl;
      std::cerr << "DEBUG: Starting new command input cycle" << std::endl;
    }
    notify_plugins("main_process_start", c_pid_str);

    // Check and handle any pending signals before prompting for input
    g_shell->process_pending_signals();

    update_terminal_title();

    // gather and create the prompt
    std::string prompt;
    if (g_shell->get_menu_active()) {
      prompt = g_shell->get_prompt();
    } else {
      prompt = g_shell->get_ai_prompt();
    }
    if (g_theme && g_theme->uses_newline()) {
      std::cout << prompt << std::endl;
      prompt = g_shell->get_newline_prompt();
    }

    char* input = ic_readline(prompt.c_str());
    if (input != nullptr) {
      std::string command(input);
      if (g_debug_mode)
        std::cerr << "DEBUG: User input: " << command << std::endl;
      ic_free(input);
      if (!command.empty()) {
        notify_plugins("main_process_command_processed", command);
        {
          std::string status_str;

          // process the command
          if (g_shell->get_menu_active()) {
            status_str = std::to_string(g_shell->execute(command));
          } else {
            if (command[0] == ':') {
              command.erase(0, 1);
              status_str = std::to_string(g_shell->execute(command));
            } else {
              status_str = std::to_string(g_shell->do_ai_request(command));
            }
          }

          if (g_debug_mode)
            std::cerr << "DEBUG: Command exit status: " << status_str
                      << std::endl;
          update_completion_frequency(command);
          ic_history_add(command.c_str());
          setenv("STATUS", status_str.c_str(), 1);
        }
      }
      if (g_exit_flag) {
        break;
      }
    } else {
      continue;
    }
    notify_plugins("main_process_end", c_pid_str);
    if (g_exit_flag) {
      std::cout << "Exiting main process loop..." << std::endl;
      break;
    }
  }
}

void notify_plugins(std::string trigger, std::string data) {
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

bool init_login_filesystem() {
  // verify and create if needed the cjsh login filesystem
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing login filesystem" << std::endl;
  try {
    if (!std::filesystem::exists(cjsh_filesystem::g_user_home_path)) {
      std::cerr << "cjsh: the users home path could not be determined."
                << std::endl;
      return false;
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating profile file" << std::endl;
      create_profile_file();
    }
  } catch (const std::exception& e) {
    std::cerr << "cjsh: Failed to initalize the cjsh login filesystem: "
              << e.what() << std::endl;
    return false;
  }
  return true;
}

void setup_environment_variables() {
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

void initialize_shell_environment() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: Initializing shell environment" << std::endl;
  }

  // Copy the shell's terminal settings to global variables for compatibility
  g_shell_terminal = g_shell->get_terminal();
  g_shell_pgid = g_shell->get_pgid();
  g_shell_tmodes = g_shell->get_terminal_modes();
  g_terminal_state_saved = g_shell->is_terminal_state_saved();
  g_job_control_enabled = g_shell->is_job_control_enabled();
}

bool init_interactive_filesystem() {
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
      std::cerr << "cjsh: the users home path could not be determined."
                << std::endl;
      return false;
    }

    // Initialize directories once
    cjsh_filesystem::initialize_cjsh_directories();

    // Create files if needed based on cached existence checks
    if (!history_exists) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating history file" << std::endl;
      std::ofstream history_file(cjsh_filesystem::g_cjsh_history_path);
      history_file.close();
    }

    // .cjshrc
    if (!source_exists) {
      if (g_debug_mode) std::cerr << "DEBUG: Creating source file" << std::endl;
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
    std::cerr << "cjsh: Failed to initalize the cjsh interactive filesystem: "
              << e.what() << std::endl;
    return false;
  }
  return true;
}

void process_profile_startup_args() {
  // gather flags from profile file and add to g_startup_args if not already
  // present
  if (g_debug_mode)
    std::cerr << "DEBUG: Processing startup args from profile" << std::endl;

  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Profile file does not exist, skipping startup args"
                << std::endl;
    return;
  }

  std::ifstream profile_file(cjsh_filesystem::g_cjsh_profile_path);
  if (!profile_file.is_open()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Failed to open profile file for startup args"
                << std::endl;
    return;
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Initial startup args before processing profile:"
              << std::endl;
    for (const auto& arg : g_startup_args) {
      std::cerr << "DEBUG:   " << arg << std::endl;
    }
  }

  std::string line;
  while (std::getline(profile_file, line)) {
    // Skip completely empty lines
    if (line.empty()) {
      continue;
    }

    // Trim leading whitespace
    line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));

    // Skip comments that aren't flag-related
    if (line.empty() ||
        (line[0] == '#' && (line.size() < 2 || line[1] != '-'))) {
      continue;
    }

    // Process lines starting with "--" (flags)
    if (line.size() >= 2 && line[0] == '-' && line[1] == '-') {
      std::string flag = line;

      // Handle comments at the end of a flag line
      size_t comment_pos = flag.find('#');
      if (comment_pos != std::string::npos) {
        flag = flag.substr(0, comment_pos);
        // Trim trailing whitespace after removing comment
        flag.erase(flag.find_last_not_of(" \t\n\r\f\v") + 1);
      }

      // Extract just the flag portion (in case there are other spaces or
      // characters)
      size_t space_pos = flag.find(' ');
      if (space_pos != std::string::npos) {
        flag = flag.substr(0, space_pos);
      }

      if (g_debug_mode)
        std::cerr << "DEBUG: Found startup argument in profile: " << flag
                  << std::endl;

      // Check if this flag already exists in startup args
      bool already_exists = false;
      for (const auto& existing_arg : g_startup_args) {
        if (existing_arg == flag) {
          already_exists = true;
          break;
        }
      }

      if (!already_exists) {
        if (g_debug_mode)
          std::cerr << "DEBUG: Adding new startup argument from profile: "
                    << flag << std::endl;
        g_startup_args.push_back(flag);
      } else {
        if (g_debug_mode)
          std::cerr << "DEBUG: Startup argument already exists, skipping: "
                    << flag << std::endl;
      }
    }
  }

  profile_file.close();

  if (g_debug_mode) {
    std::cerr << "DEBUG: Final startup args after processing profile:"
              << std::endl;
    for (const auto& arg : g_startup_args) {
      std::cerr << "DEBUG:   " << arg << std::endl;
    }
  }

  apply_profile_startup_args();
}

void apply_profile_startup_args() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Applying startup args from profile" << std::endl;

  // We process g_startup_args to apply the effects of the flags
  // This function applies the effects of flags, without changing g_startup_args
  // which is important for restart to work correctly

  for (const auto& arg : g_startup_args) {
    // Skip if not a proper flag format
    if (arg.size() < 2 || arg[0] != '-' || arg[1] != '-') {
      continue;
    }

    // Process the flag (removing any comments)
    std::string flag = arg;
    size_t comment_pos = flag.find('#');
    if (comment_pos != std::string::npos) {
      flag = flag.substr(0, comment_pos);
      flag.erase(flag.find_last_not_of(" \t\n\r\f\v") + 1);
    }

    // Extract just the flag portion (in case there are other spaces or
    // characters)
    size_t space_pos = flag.find(' ');
    if (space_pos != std::string::npos) {
      flag = flag.substr(0, space_pos);
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: Processing startup arg: " << flag << std::endl;

    // Apply the appropriate setting based on the flag
    if (flag == "--no-plugins") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling plugins from profile" << std::endl;
      config::plugins_enabled = false;
    } else if (flag == "--no-themes") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling themes from profile" << std::endl;
      config::themes_enabled = false;
    } else if (flag == "--no-ai") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling AI from profile" << std::endl;
      config::ai_enabled = false;
    } else if (flag == "--no-colors") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling colors from profile" << std::endl;
      config::colors_enabled = false;
    } else if (flag == "--no-update") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling update checks from profile" << std::endl;
      g_check_updates = false;
    } else if (flag == "--silent-updates") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Enabling silent updates from profile" << std::endl;
      g_silent_update_check = true;
    } else if (flag == "--no-titleline") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling title line from profile" << std::endl;
      g_title_line = false;
    } else if (flag == "--no-source") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Disabling source file from profile" << std::endl;
      config::source_enabled = false;
    } else if (flag == "--debug") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Enabling debug mode from profile" << std::endl;
      g_debug_mode = true;
    } else {
      if (g_debug_mode)
        std::cerr << "DEBUG: Unknown startup arg in profile: " << flag
                  << std::endl;
    }
  }
}

void process_profile_file() {
  // sourcing if in login shell
  if (g_debug_mode) std::cerr << "DEBUG: Processing profile files" << std::endl;
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
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
    create_profile_file();
    return;
  }

  process_profile_startup_args();

  g_shell->execute("source " + cjsh_filesystem::g_cjsh_profile_path.string());
}

void process_source_file() {
  // processed if in interactive shell
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path)) {
    create_source_file();
    return;
  }
  g_shell->execute("source " + cjsh_filesystem::g_cjsh_source_path.string());
}

void create_profile_file() {
  std::ofstream profile_file(cjsh_filesystem::g_cjsh_profile_path);
  if (profile_file.is_open()) {
    profile_file << "# cjsh Configuration File\n";
    profile_file << "# this file is sourced when the shell starts in login "
                    "mode and is sourced after /etc/profile and ~/.profile\n";
    profile_file << "# this file is the only one that is capable of handling "
                    "startup args\n";
    profile_file << "# Add startup args by including lines that start with -- "
                    "followed by the flag\n";
    profile_file << "# For example:\n";
    profile_file << "# --no-colors     # Disable colorized output\n";
    profile_file << "# --no-themes     # Disable themes\n";
    profile_file << "# --no-plugins    # Disable plugins\n";
    profile_file << "# --no-ai         # Disable AI features\n";
    profile_file << "# --no-source     # Don't source the .cjshrc file\n";
    profile_file.close();
  } else {
    std::cerr << "cjsh: Failed to create the configuration file." << std::endl;
  }
}

void create_source_file() {
  std::ofstream source_file(cjsh_filesystem::g_cjsh_source_path);
  if (source_file.is_open()) {
    source_file << "# cjsh Source File\n";
    source_file
        << "# this file is sourced when the shell starts in interactive mode\n";
    source_file << "# this is where your aliases, theme setup, enabled "
                   "plugins will be stored by default.\n";

    source_file << "# Alias examples\n";
    source_file << "alias ll='ls -la'\n";

    source_file << "# theme examples\n";
    source_file << "theme default\n";

    source_file << "# plugin examples\n";
    source_file << "# plugin example_plugin enable\n";
    source_file.close();
  } else {
    std::cerr << "cjsh: Failed to create the source file." << std::endl;
  }
}

bool is_parent_process_alive() {
  pid_t ppid = getppid();
  return ppid != 1;
}

std::string get_current_time_string() {
  std::time_t now = std::time(nullptr);
  std::tm* now_tm = std::localtime(&now);
  char buffer[100];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);
  return std::string(buffer);
}

std::string get_colorized_splash() {
  std::vector<std::string> splash_lines = {
      "   ______       __   _____    __  __",
      "  / ____/      / /  / ___/   / / / /",
      " / /      __  / /   \\__ \\   / /_/ / ",
      "/ /___   / /_/ /   ___/ /  / __  /  ",
      "\\____/   \\____/   /____/  /_/ /_/   ",
      "  CJ's Shell v" + c_version};

  std::string colorized_splash;
  if (colors::g_color_capability == colors::ColorCapability::NO_COLOR) {
    for (const auto& line : splash_lines) {
      colorized_splash += line + "\n";
    }
    return colorized_splash;
  }
  std::vector<std::pair<colors::RGB, colors::RGB>> line_gradients = {
      {colors::RGB(255, 0, 127), colors::RGB(0, 255, 255)},  // Pink to Cyan
      {colors::RGB(0, 255, 255), colors::RGB(255, 0, 255)},  // Cyan to Magenta
      {colors::RGB(255, 0, 255),
       colors::RGB(255, 255, 0)},  // Magenta to Yellow
      {colors::RGB(255, 255, 0), colors::RGB(0, 127, 255)},  // Yellow to Azure
      {colors::RGB(0, 127, 255), colors::RGB(255, 0, 127)},  // Azure to Pink
      {colors::RGB(255, 128, 0),
       colors::RGB(0, 255, 128)}  // Orange to Spring Green
  };

  for (size_t i = 0; i < splash_lines.size(); i++) {
    colorized_splash +=
        colors::gradient_text(splash_lines[i], line_gradients[i].first,
                              line_gradients[i].second) +
        "\n";
  }

  return colorized_splash;
}