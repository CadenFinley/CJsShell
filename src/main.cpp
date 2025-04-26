#include "main.h"

int main(int argc, char *argv[]) {
  // cjsh

  // this handles the prompting and executing of commands
  g_shell = new Shell(c_pid);

  // Setup signal handlers before anything else
  setup_signal_handlers();
  
  // Initialize login environment if necessary
  if (g_shell->get_login_mode()) {
    initialize_login_environment();
    setup_environment_variables();
    setup_job_control();
  }

  g_cjsh_path = ""; // need to determine where cjsh is installed at TODO
  if (g_cjsh_path.empty()) {
    std::cerr << "cjsh: Failed to determine the cjsh installation path." << std::endl;
    return 1;
  }

  if (g_shell->get_login_mode()) {
    if (!init_login_filesystem()) {
      std::cerr << "Error: Failed to initialize or verify file system or files within the file system." << std::endl;
      return 1;
    }
  }

  // check if this is getting used to run a executable
  // if so then we need to run the executable and exit

  // check for non interactive command line arguments
  // -c, --command
  // -v, --version
  // -h, --help
  // --set-as-shell
  // --update
  bool l_execute_command = false;
  std::string l_cmd_to_execute = "";
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    switch (arg) {
      case "-c","--command":
        l_execute_command = true;
        l_cmd_to_execute = argv[i + 1];
        i++;
        break;
      case "-v","--version":
        std::cout << c_version << std::endl;
        return 0;
        break;
      case "-h","--help":
        return 0;
        break;
      case "--set-as-shell": //TODO
        // somehow handle setting this as the default shell
        return 0;
        break;
      case "--update": //TODO
        // somehow handle updating the shell
        return 0;
        break;
    }
  }

  // execute the command passed in the startup arg and exit
  if (l_execute_command) {
    g_shell->execute_command(l_cmd_to_execute, true);
    delete g_shell;
    g_shell = nullptr;
    return 0;
  }

  // now we know we are in interactive mode
  g_shell->set_interactive_mode(true);
  std::thread watchdog_thread(parent_process_watchdog);
  watchdog_thread.detach();

  // initialize and verify the file system
  if (!init_interactive_filesystem()) {
    std::cerr << "Error: Failed to initialize or verify file system or files within the file system." << std::endl;
    return 1;
  }

  // check for interactive command line arguments
  // --no-update
  // -d, --debug
  // --check-update
  // --no-source
  // --no-titleline
  // --no-plugin
  // --no-theme
  // --no-ai

  bool l_load_plugin = true;
  bool l_load_theme = true;
  bool l_load_ai = true;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    switch (arg) {
      case "--no-update":
        g_check_updates = false;
        break;
      case "-d","--debug":
        g_debug_mode = true;
        break;
      case "--check-update":
        g_check_updates = true;
        break;
      case "--no-source":
        g_source = false;
        break;
      case "--no-titleline":
        g_title_line = false;
        break;
      case "--no-plugin":
        l_load_plugin = false;
        break;
      case "--no-theme":
        l_load_theme = false;
        break;
      case "--no-ai":
        l_load_ai = false;
        break;
    }
  }

  // initalize objects
  if (l_load_plugin) {
    // this will load the users plugins from .cjsh_data/plugins
    g_plugin = new Plugin(); //doesnt need to verify filesys
  }
  if (l_load_theme) {
    // this will load the users selected theme from .cjshrc
    g_theme = new Theme(); //doesnt need to verify filesys
  }
  if (l_load_ai) {
    g_ai = new AI("", "chat", "You are an AI personal assistant within a users login shell.", {}, g_cjsh_data_path);
  }

  // TODO
  // check if a changelog file exists
  // if so then a update was just installed so no need to check for updates
  // and then display changelog

  // check for updates if enabled
  // if enabled check update cache
  // if not cached, check for updates
  // if update available, prompt user to update

  if(!g_shell->get_exit_flag()) {
    if (g_title_line) {
      std::cout << c_title_color << title_line << c_reset_color << std::endl;
      std::cout << c_title_color << created_line << c_reset_color << std::endl;
    }

    main_process_loop();
  }

  std::cout << "CJ's Shell Exiting..." << std::endl;

  if (g_shell->get_login_mode()) {
    restore_terminal_state();
  }

  delete g_shell;
  g_shell = nullptr;
  delete g_ai;
  g_ai = nullptr;
  delete g_theme;
  g_theme = nullptr;
  delete g_plugin;
  g_plugin = nullptr;

  // clean up main
  return 0;
}

// Signal handling functions
void setup_signal_handlers() {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sigset_t block_mask;
  sigfillset(&block_mask);

  // Setup SIGHUP handler (terminal disconnect)
  sa.sa_sigaction = signal_handler_wrapper;
  sa.sa_mask = block_mask;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGHUP, &sa, nullptr);

  // Setup SIGTERM handler
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGTERM, &sa, nullptr);

  // Setup SIGCHLD handler (child process state changes)
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGCHLD, &sa, nullptr);

  // Setup SIGINT handler (Ctrl+C)
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGINT, &sa, nullptr);

  // Ignore certain signals
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sa.sa_mask = block_mask;
  sigaction(SIGQUIT, &sa, nullptr);
  sigaction(SIGTSTP, &sa, nullptr);
  sigaction(SIGTTIN, &sa, nullptr);
  sigaction(SIGTTOU, &sa, nullptr);

  save_terminal_state();
}

static void signal_handler_wrapper(int signum, siginfo_t* info, void* context) {
  switch (signum) {
    case SIGHUP:
      std::cerr << "Received SIGHUP, terminal disconnected" << std::endl;
      g_shell->set_exit_flag(true);
      
      // Clean up and exit immediately
      if (g_job_control_enabled) {
        try {
          restore_terminal_state();
        } catch (...) {}
      }
      
      _exit(0);
      break;
      
    case SIGTERM:
      std::cerr << "Received SIGTERM, exiting" << std::endl;
      g_shell->set_exit_flag(true);
      _exit(0);
      break;
      
    case SIGINT:
      std::cerr << "Received SIGINT, interrupting current operation" << std::endl;
      break;
      
    case SIGCHLD:
      // Reap zombie processes
      pid_t child_pid;
      int status;
      while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Child process terminated
      }
      break;
  }
}

void save_terminal_state() {
  if (isatty(STDIN_FILENO)) {
    if (tcgetattr(STDIN_FILENO, &g_original_termios) == 0) {
      g_terminal_state_saved = true;
    }
  }
}

void restore_terminal_state() {
  if (g_terminal_state_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
  }
}

void setup_job_control() {
  if (!isatty(STDIN_FILENO)) {
    g_job_control_enabled = false;
    return;
  }
  
  g_shell_pgid = getpid();
  
  if (setpgid(g_shell_pgid, g_shell_pgid) < 0) {
    if (errno != EPERM) {
      perror("Couldn't put the shell in its own process group");
    }
  }
  
  try {
    g_shell_terminal = STDIN_FILENO;
    int tpgrp = tcgetpgrp(g_shell_terminal);
    if (tpgrp != -1) {
      if (tcsetpgrp(g_shell_terminal, g_shell_pgid) < 0) {
        perror("Couldn't grab terminal control");
      }
    }
    
    if (tcgetattr(g_shell_terminal, &g_shell_tmodes) < 0) {
      perror("Couldn't get terminal attributes");
    }
    
    g_job_control_enabled = true;
  } catch (const std::exception& e) {
    std::cerr << "Error setting up terminal: " << e.what() << std::endl;
    g_job_control_enabled = false;
  }
}

void setup_environment_variables() {
  uid_t uid = getuid();
  struct passwd* pw = getpwuid(uid);
  
  if (pw != nullptr) {
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);
    setenv("SHELL", g_cjsh_path.c_str(), 1);
    
    setenv("CJSH_VERSION", c_version.c_str(), 1);
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      setenv("HOSTNAME", hostname, 1);
    }
    
    setenv("PWD", std::filesystem::current_path().string().c_str(), 1);
    
    if (getenv("TZ") == nullptr) {
      std::string tz_file = "/etc/localtime";
      if (std::filesystem::exists(tz_file)) {
        setenv("TZ", tz_file.c_str(), 1);
      }
    }
  }
}

void initialize_login_environment() {
  pid_t pid = getpid();
  
  if (setsid() < 0) {
    if (errno != EPERM) {
      perror("Failed to become session leader");
    }
  }
  
  g_shell_terminal = STDIN_FILENO;
  
  if (!isatty(g_shell_terminal)) {
    std::cerr << "Warning: Not running on a terminal device" << std::endl;
  }
}

void main_process_loop() {
  notify_plugins("main_process_pre_run", g_pid);

  ic_set_prompt_marker("", NULL);
  ic_enable_hint(true);
  ic_set_hint_delay(100);
  ic_enable_completion_preview(true);

  while(true) {
    notify_plugins("main_process_start", g_pid);
    if (g_debug_mode) {
      std::cout << g_theme->debug_color << "DEBUG MODE ENABLED" << c_reset_color << std::endl;
    }

    std::string prompt;
    if (g_menu_terminal) {
      prompt = g_shell->get_prompt();
    } else {
      prompt = g_theme->get_ai_prompt();
    }
    prompt += " ";
    char* input = ic_readline(prompt.c_str());
    if (input != nullptr || *input == 'exit') {
      std::string command(input);
      ic_free(input);
      if (!command.empty()) {
        notify_plugins("main_process_command_processed", input);
        ic_add_history(input);
        g_shell->execute_command(input, true);
      }
      if (g_shell->get_exit_flag()) {
        break;
      }
    } else {
      g_shell->set_exit_flag(true);
    }
    notify_plugins("main_process_end", g_pid);
    if (g_shell->get_exit_flag()) {
      break;
    }
  }
}

void notify_plugins(std::string trigger, std::string data) {
  if (g_plugin == nullptr) {
    g_shell->set_exit_flag(true);
    std::cerr << "Error: Plugin system not initialized." << std::endl;
    return;
  }
  if (g_plugin->get_enabled_plugins().empty()) {
    return;
  }
  if (g_debug_mode) {
    std::cout << "DEBUG: Notifying plugins of trigger: " << trigger << std::endl;
  }
  g_plugin->trigger_subscribed_global_event(trigger, data);
}

bool init_login_filesystem() {
  try {
    if (!std::filesystem::exists(g_user_home_path)) {
      std::cerr << "cjsh: the users home path could not be determined." << std::endl;
      return false;
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_config_path)) {
      create_config_file();
    }
    process_config_file();
  } catch (const std::exception& e) {
    std::cerr << "cjsh: Failed to initalize the cjsh login filesystem: " + e.what() << std::endl;
    return false;
  }
  return true;
}

bool init_interactive_filesystem() {
  try {
    if (!std::filesystem::exists(g_user_home_path)) {
      std::cerr << "cjsh: the users home path could not be determined." << std::endl;
      return false;
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_data_path)) {
      std::filesystem::create_directories(cjsh_filesystem::g_cjsh_data_path);
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_plugin_path)) {
      std::filesystem::create_directories(cjsh_filesystem::g_cjsh_plugin_path);
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_theme_path)) {
      std::filesystem::create_directories(cjsh_filesystem::g_cjsh_theme_path);
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_history_path)) {
      std::ofstream history_file(cjsh_filesystem::g_cjsh_history_path);
      history_file.close();
    }
  } catch (const std::exception& e) {
    std::cerr << "cjsh: Failed to initalize the cjsh interactive filesystem: " + e.what() << std::endl;
    return false;
  }
  return true;
}

void process_config_file() {
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_config_path)) {
    create_config_file();
    return;
  }

  std::ifstream config_file(cjsh_filesystem::g_cjsh_config_path);
  if (!config_file.is_open()) {
    std::cerr << "cjsh: Failed to open the configuration file for reading." << std::endl;
    return;
  }

  std::string line;
  while (std::getline(config_file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (line.find("export ") == 0 || line.find('=') != std::string::npos) {
      size_t equal_pos = line.find('=');
      if (equal_pos != std::string::npos) {
        std::string var_name = line.substr(0, equal_pos);
        std::string var_value = line.substr(equal_pos + 1);
        
        if (var_name.find("export ") == 0) {
          var_name = var_name.substr(7);
        }
        
        var_name.erase(0, var_name.find_first_not_of(" \t"));
        var_name.erase(var_name.find_last_not_of(" \t") + 1);
        var_value.erase(0, var_value.find_first_not_of(" \t"));
        var_value.erase(var_value.find_last_not_of(" \t") + 1);
        
        if ((var_value.front() == '"' && var_value.back() == '"') || 
            (var_value.front() == '\'' && var_value.back() == '\'')) {
          var_value = var_value.substr(1, var_value.length() - 2);
        }
        
        setenv(var_name.c_str(), var_value.c_str(), 1);
      }
    }
    else if (line.find("PATH=") == 0 || line.find("PATH=$PATH:") == 0) {
      g_shell->execute_command(line, true);
    }
    else {
      g_shell->execute_command(line, true);
    }
  }
  
  config_file.close();
}

void process_source_file() {
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path)) {
    create_source_file();
    return;
  }

  std::ifstream source_file(cjsh_filesystem::g_cjsh_source_path);
  if (!source_file.is_open()) {
    std::cerr << "cjsh: Failed to open the source file for reading." << std::endl;
    return;
  }

  std::string line;
  while (std::getline(source_file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (line.find("alias ") == 0) {
      g_shell->execute_command(line, false);
    }
    else if (line.find("theme ") == 0) {
      g_current_theme = line.substr(6);
      g_current_theme.erase(0, g_current_theme.find_first_not_of(" \t"));
      g_current_theme.erase(g_current_theme.find_last_not_of(" \t") + 1);
    }
    else if (line.find("plugin ") == 0) {
      if (g_plugin != nullptr) {
        std::string plugin_cmd = line.substr(7);
        std::stringstream ss(plugin_cmd);
        std::string plugin_name, plugin_action;
        ss >> plugin_name >> plugin_action;
        
        if (plugin_action == "enable" || plugin_action == "on") {
          g_plugin->enable_plugin(plugin_name);
        } else if (plugin_action == "disable" || plugin_action == "off") {
          g_plugin->disable_plugin(plugin_name);
        }
      }
    }
    else {
      g_shell->execute_command(line, true);
    }
  }
  
  source_file.close();
}

void create_config_file() {
  std::ofstream config_file(cjsh_filesystem::g_cjsh_config_path);
  if (config_file.is_open()) {
    config_file << "# CJ's Shell Configuration File\n";
    config_file << "# This file is sourced when the shell starts in login mode\n";
    config_file << "# This file is used to configure the shell PATH and set environment variables.\n";
    config_file << "# Do not edit this file unless you know what you are doing.\n";

    config_file << "if [ -x /usr/libexec/path_helper ]; then\n";
    config_file << "eval /usr/libexec/path_helper -s\n";
    config_file << "fi\n\n";

    config_file << "# Any environment variables will be set here.\n";
    config_file.close();
  } else {
    std::cerr << "cjsh: Failed to create the configuration file." << std::endl;
  }
}

void create_source_file() {
  std::ofstream source_file(cjsh_filesystem::g_cjsh_source_path);
  if (source_file.is_open()) {
    source_file << "# CJ's Shell Source File\n";
    source_file << "# This file is sourced when the shell starts in interactive mode\n";
    source_file << "# This is where your aliases, theme setup, enabled plugins, startup commands, functions etc.";
    source_file << "# are stored.\n";

    source_file << "# Do not edit this file unless you know what you are doing.\n";
    source_file << "# Alias examples\n";
    source_file << "alias ll='ls -la'\n";
    source_file << "alias gs='git status'\n";

    source_file << "# Theme examples\n";
    source_file << "theme default\n";

    source_file << "# Plugin examples\n";
    source_file << "# plugin example_plugin enable\n";
    source_file.close();
  } else {
    std::cerr << "cjsh: Failed to create the source file." << std::endl;
  }
}

void parent_process_watchdog() {
  while (!g_shell->get_exit_flag()) {
    if (!is_parent_process_alive()) {
      std::cerr << "Parent process terminated, shutting down..." << std::endl;
        g_shell->set_exit_flag(true);
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}

bool is_parent_process_alive() {
  pid_t ppid = getppid();
  return ppid != 1;
}
