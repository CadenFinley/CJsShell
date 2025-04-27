#include "main.h"
#include "../isocline/include/isocline.h"
#include "cjsh_filesystem.h"
#include <signal.h>
#include "colors.h"

int main(int argc, char *argv[]) {
  // cjsh
  
  // verify installation
  if (!initialize_cjsh_path()){
    std::cerr << "Warning: Unable to determine the executable path. This program may not work correctly." << std::endl;
  }
  
  // Initialize signal handling before creating the shell
  prepare_shell_signal_environment();
  
  // this handles the prompting and executing of commands
  g_shell = new Shell(argv);
  
  // setup signal handlers through the shell
  g_shell->setup_signal_handlers();
  
  // Initialize login environment if necessary
  if (g_shell->get_login_mode()) {
    if (!init_login_filesystem()) {
      std::cerr << "Error: Failed to initialize or verify file system or files within the file system." << std::endl;
      return 1;
    }
    process_config_file();
    initialize_login_environment();
    setup_environment_variables();
    g_shell->setup_job_control();
  }

  // Initialize color support
  colors::initialize_color_support();
  
  // check for non interactive command line arguments
  // -c, --command
  // -v, --version
  // -h, --help
  // --set-as-shell
  // --update
  // --silent-updates
  // --splash
  bool l_execute_command = false;
  std::string l_cmd_to_execute = "";
  
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-c" || arg == "--command") {
      if (i + 1 < argc) {
        l_execute_command = true;
        l_cmd_to_execute = argv[i + 1];
        i++;
      }
    }
    else if (arg == "-v" || arg == "--version") {
      std::cout << c_version << std::endl;
      return 0;
    }
    else if (arg == "-h" || arg == "--help") {
      g_shell ->execute_command("help", true);
      return 0;
    }
    else if (arg == "--set-as-shell") {
      std::cout << "Setting CJ's Shell as the default shell..." << std::endl;
      std::cerr << "Please run the following command to set CJ's Shell as your default shell:\n";
      std::cerr << "chsh -s " << cjsh_filesystem::g_cjsh_path << std::endl;
      return 0;
    }
    else if (arg == "--force-update") {
      download_latest_release();
      return 0;
    }
    else if (arg == "--update") {
      execute_update_if_available(check_for_update());
      return 0;
    }
    else if (arg == "--silent-updates") {
      g_silent_update_check = true;
    }
    else if (arg == "--splash") {
      std::cout << get_colorized_splash() << std::endl;
      return 0;
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

  // set process watchdog
  std::thread watchdog_thread(parent_process_watchdog);
  watchdog_thread.detach();

  // set initial working directory
  setenv("PWD", std::filesystem::current_path().string().c_str(), 1);

  // check for interactive command line arguments
  // --no-update
  // -d, --debug
  // --check-update
  // --no-source
  // --no-titleline
  // --no-plugin
  // --no-ai

  bool l_load_plugin = true;
  bool l_load_theme = true;
  bool l_load_ai = true;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--no-update") {
      g_check_updates = false;
    }
    else if (arg == "-d" || arg == "--debug") {
      g_debug_mode = true;
    }
    else if (arg == "--check-update") {
      g_check_updates = true;
    }
    else if (arg == "--no-source") {
      g_source = false;
    }
    else if (arg == "--no-titleline") {
      g_title_line = false;
    }
    else if (arg == "--no-plugin") {
      l_load_plugin = false;
    }
    else if (arg == "--no-ai") {
      l_load_ai = false;
    } else if (arg.length() > 0 && arg[0] == '-') {
      std::cerr << "Warning: Unknown argument: " << arg << std::endl;
    }
  }

  // initialize and verify the file system
  if (!init_interactive_filesystem()) {
    std::cerr << "Error: Failed to initialize or verify file system or files within the file system." << std::endl;
    return 1;
  }

  // initalize objects
  if (l_load_plugin) {
    // this will load the users plugins from .cjsh_data/plugins
    g_plugin = new Plugin(cjsh_filesystem::g_cjsh_plugin_path); //doesnt need to verify filesys
  }
  if (l_load_theme) {
    // this will load the users selected theme from .cjshrc
    g_theme = new Theme(cjsh_filesystem::g_cjsh_theme_path); //doesnt need to verify filesys
  }
  process_source_file();
  if (l_load_ai) {
    // Get API key from environment if available
    std::string api_key = "";
    const char* env_key = getenv("OPENAI_API_KEY");
    if (env_key) {
      api_key = env_key;
    }
    g_ai = new Ai(api_key, "chat", "You are an AI personal assistant within a users login shell.", {}, cjsh_filesystem::g_cjsh_data_path);
  }

  if(!g_exit_flag) {
    // do update process
    startup_update_process();
    if (g_title_line) {
      std::cout << title_line << std::endl;
      std::cout << created_line  << std::endl;
    }

    main_process_loop();
  }

  std::cout << "CJ's Shell Exiting..." << std::endl;

  if (g_shell->get_login_mode()) {
    g_shell->restore_terminal_state();
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

void update_terminal_title() {
  std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
  std::cout.flush();
}

void main_process_loop() {
  notify_plugins("main_process_pre_run", c_pid_str);

  ic_set_prompt_marker("", NULL);
  ic_enable_hint(true);
  ic_set_hint_delay(100);
  ic_enable_completion_preview(true);
  ic_set_history(cjsh_filesystem::g_cjsh_history_path.c_str(), -1);

  while(true) {
    notify_plugins("main_process_start", c_pid_str);
    if (g_debug_mode) {
      std::cout << c_title_color << "DEBUG MODE ENABLED" << c_reset_color << std::endl;
    }
    update_terminal_title();
    
    std::string prompt;
    if (g_menu_terminal) {
      prompt = g_shell->get_prompt();
    } else {
      prompt = g_shell->get_ai_prompt();
    }
    prompt += " ";
    char* input = ic_readline(prompt.c_str());
    if (input != nullptr) {
      std::string command(input);
      ic_free(input);
      if (!command.empty()) {
        notify_plugins("main_process_command_processed", command);
        ic_history_add(command.c_str());
        g_shell->execute_command(command, true);
        update_terminal_title();
      }
      if (g_exit_flag) {
        break;
      }
    } else {
      g_exit_flag = true;
    }
    notify_plugins("main_process_end", c_pid_str);
    if (g_exit_flag) {
      break;
    }
  }
}

void notify_plugins(std::string trigger, std::string data) {
  if (g_plugin == nullptr) {
    g_exit_flag = true;
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
    if (!std::filesystem::exists(cjsh_filesystem::g_user_home_path)) {
      std::cerr << "cjsh: the users home path could not be determined." << std::endl;
      return false;
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
      create_config_file();
    }
  } catch (const std::exception& e) {
    std::cerr << "cjsh: Failed to initalize the cjsh login filesystem: " << e.what() << std::endl;
    return false;
  }
  return true;
}

void setup_environment_variables() {
  uid_t uid = getuid();
  struct passwd* pw = getpwuid(uid);
  
  if (pw != nullptr) {
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);
    
    if (getenv("PATH") == nullptr) {
      setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      setenv("HOSTNAME", hostname, 1);
    }
    
    setenv("PWD", std::filesystem::current_path().string().c_str(), 1);
  }
}

void initialize_login_environment() {
  if (g_shell->get_login_mode()) {
    g_shell_terminal = STDIN_FILENO;
    
    if (!isatty(g_shell_terminal)) {
      std::cerr << "Warning: Not running on a terminal device" << std::endl;
    }
    
    g_shell_pgid = getpid();

    if (setpgid(g_shell_pgid, g_shell_pgid) < 0) {
      if (errno != EPERM) {
        perror("setpgid failed");
      }
    }
    
    if (isatty(g_shell_terminal)) {
      if (tcsetpgrp(g_shell_terminal, g_shell_pgid) < 0) {
        perror("tcsetpgrp failed in initialize_login_environment");
      }
    }
    
    struct termios term_attrs;
    if (tcgetattr(g_shell_terminal, &term_attrs) == 0) {
      g_shell_tmodes = term_attrs;
      g_terminal_state_saved = true;
    }
  }
}

void prepare_shell_signal_environment() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGQUIT);
  sigaddset(&mask, SIGTSTP);
  sigaddset(&mask, SIGTTIN);
  sigaddset(&mask, SIGTTOU);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, nullptr);
  
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  
  sigaction(SIGTTOU, &sa, nullptr);
  sigaction(SIGTTIN, &sa, nullptr);
  sigaction(SIGQUIT, &sa, nullptr);
  sigaction(SIGTSTP, &sa, nullptr);
  
  sigprocmask(SIG_UNBLOCK, &mask, nullptr);
}

bool init_interactive_filesystem() {
  try {
    if (!std::filesystem::exists(cjsh_filesystem::g_user_home_path)) {
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
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path)) {
      create_source_file();
    }
  } catch (const std::exception& e) {
    std::cerr << "cjsh: Failed to initalize the cjsh interactive filesystem: " << e.what() << std::endl;
    return false;
  }
  return true;
}

void process_config_file() {
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
    create_config_file();
    return;
  }

  std::ifstream config_file(cjsh_filesystem::g_cjsh_profile_path);
  if (!config_file.is_open()) {
    std::cerr << "cjsh: Failed to open the configuration file for reading." << std::endl;
    return;
  }
  
  config_file.clear();
  config_file.seekg(0);
  
  std::string line;
  while (std::getline(config_file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    
    if (line == "PATH_HELPER") {
      process_shell_scripts_in_config();
    }
  }
  
  config_file.clear();
  config_file.seekg(0);
  
  while (std::getline(config_file, line)) {
    if (line.empty() || line[0] == '#' || line == "PATH_HELPER") {
      continue;
    }

    if (is_shell_script_construct(line)) {
      continue;
    }
    
    if (parse_and_set_env_var(line)) {
      continue;
    }
    
    g_shell->execute_command(line, true);
  }
  
  config_file.close();
  if (g_debug_mode) {
    std::cout << "DEBUG: Configuration file processed." << std::endl;
  }
}

bool is_shell_script_construct(const std::string& line) {
  std::string trimmed = line;
  trimmed.erase(0, trimmed.find_first_not_of(" \t"));
  
  if (trimmed.find("if ") == 0 || 
      trimmed == "then" || 
      trimmed == "else" || 
      trimmed == "elif" || 
      trimmed == "fi" || 
      trimmed.find("eval ") == 0 ||
      trimmed.find("for ") == 0 ||
      trimmed.find("while ") == 0 ||
      trimmed == "do" ||
      trimmed == "done") {
    return true;
  }
  return false;
}

void process_shell_scripts_in_config() {
  if (std::filesystem::exists("/usr/libexec/path_helper")) {
    FILE* pipe = popen("/usr/libexec/path_helper -s", "r");
    if (pipe) {
      char buffer[256];
      std::string result;
      
      while (!feof(pipe)) {
        if (fgets(buffer, 256, pipe) != NULL) {
          result += buffer;
        }
      }
      pclose(pipe);
      
      std::string path_value;
      size_t pos = result.find("PATH=\"");
      if (pos != std::string::npos) {
        pos += 6;
        size_t end_pos = result.find("\"", pos);
        if (end_pos != std::string::npos) {
          path_value = result.substr(pos, end_pos - pos);
          if (!path_value.empty()) {
            setenv("PATH", path_value.c_str(), 1);
            if (g_debug_mode) {
              std::cout << "DEBUG: Set PATH=" << path_value << std::endl;
            }
          }
        }
      }
    }
  }
  else {
    std::cerr << "Warning: /usr/libexec/path_helper not found. PATH may not be set correctly." << std::endl;
  }
  if (g_debug_mode) {
    std::cout << "DEBUG: PATH_HELPER executed." << std::endl;
  }
}

bool parse_and_set_env_var(const std::string& line) {
  if (line.find("export ") == 0) {
    std::string var_setting = line.substr(7);
    size_t equals_pos = var_setting.find('=');
    
    if (equals_pos != std::string::npos) {
      std::string var_name = var_setting.substr(0, equals_pos);
      std::string var_value = var_setting.substr(equals_pos + 1);
      
      if (var_value.size() >= 2 && 
          ((var_value.front() == '"' && var_value.back() == '"') || 
           (var_value.front() == '\'' && var_value.back() == '\''))) {
        var_value = var_value.substr(1, var_value.size() - 2);
      }
      
      setenv(var_name.c_str(), var_value.c_str(), 1);
      
      if (g_debug_mode) {
        std::cout << "DEBUG: Set " << var_name << "=" << var_value << std::endl;
      }
      return true;
    }
  }
  else {
    size_t equals_pos = line.find('=');
    
    if (equals_pos != std::string::npos && equals_pos > 0) {
      std::string var_name = line.substr(0, equals_pos);
      std::string var_value = line.substr(equals_pos + 1);
      
      if (var_value.size() >= 2 && 
          ((var_value.front() == '"' && var_value.back() == '"') || 
           (var_value.front() == '\'' && var_value.back() == '\''))) {
        var_value = var_value.substr(1, var_value.size() - 2);
      }
      
      setenv(var_name.c_str(), var_value.c_str(), 1);
      
      if (g_debug_mode) {
        std::cout << "DEBUG: Set " << var_name << "=" << var_value << std::endl;
      }
      return true;
    }
  }
  
  return false;
}

void create_config_file() {
  std::ofstream config_file(cjsh_filesystem::g_cjsh_profile_path);
  if (config_file.is_open()) {
    config_file << "# CJ's Shell Configuration File\n";
    config_file << "# This file is sourced when the shell starts in login mode\n";
    config_file << "# This file is used to configure the shell PATH and set environment variables.\n";
    config_file << "# Do not edit this file unless you know what you are doing.\n\n";

    config_file << "# Path helper command - sets up system PATH\n";
    config_file << "PATH_HELPER\n\n";

    config_file << "# Any environment variables should be set without 'export' command\n";
    config_file << "# Example: VARIABLE=value\n";
    config_file.close();
  } else {
    std::cerr << "cjsh: Failed to create the configuration file." << std::endl;
  }
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
      g_shell->execute_command(line, true);
    }
    else if (line.find("theme ") == 0) {
      g_shell->execute_command(line, true);
    }
    else if (line.find("plugin ") == 0) {
      g_shell->execute_command(line, true);
    }
    else {
      g_shell->execute_command(line, true);
    }
  }
  source_file.close();
  if (g_debug_mode) {
    std::cout << "DEBUG: Source file processed." << std::endl;
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
  while (!g_exit_flag) {
    if (!is_parent_process_alive()) {
      std::cerr << "Parent process terminated, shutting down..." << std::endl;
      g_exit_flag = true;
      if(g_shell){
        delete g_shell;
        g_shell = nullptr;
      }
      if(g_ai){
        delete g_ai;
        g_ai = nullptr;
      }
      if(g_plugin){
        delete g_plugin;
        g_plugin = nullptr;
      }
      if(g_theme){
        delete g_theme;
        g_theme = nullptr;
      }
      _exit(1);
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
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

bool is_newer_version(const std::string& latest, const std::string& current) {
  std::vector<int> splitVersion = [](const std::string &ver) {
      std::vector<int> parts;
      std::istringstream iss(ver);
      std::string token;
      while (std::getline(iss, token, '.')) {
          parts.push_back(std::stoi(token));
      }
      return parts;
  }(latest);
  
  std::vector<int> currentParts = [](const std::string &ver) {
      std::vector<int> parts;
      std::istringstream iss(ver);
      std::string token;
      while (std::getline(iss, token, '.')) {
          parts.push_back(std::stoi(token));
      }
      return parts;
  }(current);
  
  size_t len = std::max(splitVersion.size(), currentParts.size());
  splitVersion.resize(len, 0);
  currentParts.resize(len, 0);
  
  for (size_t i = 0; i < len; i++) {
      if (splitVersion[i] > currentParts[i]) return true;
      if (splitVersion[i] < currentParts[i]) return false;
  }
  
  return false;
}

bool check_for_update() {
  if (!g_silent_update_check) {
      std::cout << "Checking for updates from GitHub...";
  }
  
  std::string command = "curl -s " + c_update_url;
  std::string result;
  FILE *pipe = popen(command.c_str(), "r");
  
  if (!pipe) {
      std::cerr << "Error: Unable to execute update check or no internet connection." << std::endl;
      return false;
  }
  
  char buffer[128];
  while (fgets(buffer, 128, pipe) != nullptr) {
      result += buffer;
  }
  pclose(pipe);

  try {
      json jsonData = json::parse(result);
      if (jsonData.contains("tag_name")) {
          std::string latestTag = jsonData["tag_name"].get<std::string>();
          if (!latestTag.empty() && latestTag[0] == 'v') {
              latestTag = latestTag.substr(1);
          }
          
          std::string currentVer = c_version;
          if (!currentVer.empty() && currentVer[0] == 'v') {
              currentVer = currentVer.substr(1);
          }
          
          g_cached_version = latestTag;
          
          if (is_newer_version(latestTag, currentVer)) {
              std::cout << "\nLast Updated: " << g_last_updated << std::endl;
              std::cout << c_version << " -> " << latestTag << std::endl;
              return true;
          }
      }
  } catch (const std::exception& e) {
      std::cerr << "Error parsing update data: " << e.what() << std::endl;
  }
  
  return false;
}

bool load_update_cache() {
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_update_cache_path)) {
      return false;
  }
  
  std::ifstream cacheFile(cjsh_filesystem::g_cjsh_update_cache_path);
  if (!cacheFile.is_open()) return false;
  
  try {
      json cacheData;
      cacheFile >> cacheData;
      cacheFile.close();
      
      if (cacheData.contains("update_available") && 
          cacheData.contains("latest_version") && 
          cacheData.contains("check_time")) {
          
          g_cached_update = cacheData["update_available"].get<bool>();
          g_cached_version = cacheData["latest_version"].get<std::string>();
          g_last_update_check = cacheData["check_time"].get<time_t>();
          
          return true;
      }
      return false;
  } catch (const std::exception& e) {
      std::cerr << "Error loading update cache: " << e.what() << std::endl;
      return false;
  }
}

void save_update_cache(bool update_available, const std::string& latest_version) {
  json cacheData;
  cacheData["update_available"] = update_available;
  cacheData["latest_version"] = latest_version;
  cacheData["check_time"] = std::time(nullptr);
  
  std::ofstream cacheFile(cjsh_filesystem::g_cjsh_update_cache_path);
  if (cacheFile.is_open()) {
      cacheFile << cacheData.dump();
      cacheFile.close();
      
      g_cached_update = update_available;
      g_cached_version = latest_version;
      g_last_update_check = std::time(nullptr);
  } else {
      std::cerr << "Warning: Could not open update cache file for writing." << std::endl;
  }
}

bool should_check_for_updates() {
  if (g_last_update_check == 0) {
      return true;
  }
  
  time_t current_time = std::time(nullptr);
  time_t elapsed_time = current_time - g_last_update_check;
  
  if (g_debug_mode) {
      std::cout << "Time since last update check: " << elapsed_time 
                << " seconds (interval: " << g_update_check_interval << " seconds)" << std::endl;
  }
  
  return elapsed_time > g_update_check_interval;
}

bool download_latest_release() {
  std::cout << "Downloading latest release..." << std::endl;
  
  std::filesystem::path temp_dir = cjsh_filesystem::g_cjsh_data_path / "temp_update";
  if (std::filesystem::exists(temp_dir)) {
      std::filesystem::remove_all(temp_dir);
  }
  std::filesystem::create_directory(temp_dir);

  std::string download_url;
  {
      std::string api_cmd = "curl -s " + c_update_url;
      std::string api_result;
      FILE* api_pipe = popen(api_cmd.c_str(), "r");
      if (api_pipe) {
          char buf[128];
          while (fgets(buf, sizeof(buf), api_pipe)) api_result += buf;
          pclose(api_pipe);
          try {
              auto api_json = json::parse(api_result);

              std::string os_suffix;
              #if defined(__APPLE__)
              os_suffix = "macos";
              #elif defined(__linux__)
              os_suffix = "linux";
              #else
              os_suffix = "";
              #endif
              std::string expected = "cjsh" + (os_suffix.empty() ? "" : "-" + os_suffix);

              for (const auto& asset : api_json["assets"]) {
                  std::string name = asset.value("name", "");
                  if (name == expected || (download_url.empty() && name == "cjsh")) {
                      download_url = asset.value("browser_download_url", "");
                      if (name == expected) break;
                  }
              }
          } catch (const std::exception& e) {
              std::cerr << "Error parsing asset data: " << e.what() << std::endl;
          }
      }
  }
  
  if (download_url.empty()) {
      std::cerr << "Error: Unable to determine download URL." << std::endl;
      return false;
  }

  std::string asset_name = download_url.substr(download_url.find_last_of('/') + 1);
  std::filesystem::path asset_path = temp_dir / asset_name;
  std::string curl_command = "curl -L -s " + download_url + " -o " + asset_path.string();
  
  FILE* download_pipe = popen(curl_command.c_str(), "r");
  if (!download_pipe) {
      std::cerr << "Error: Failed to execute download command." << std::endl;
      return false;
  }
  pclose(download_pipe);

  if (!std::filesystem::exists(asset_path)) {
      std::cerr << "Error: Download failed - output file not created." << std::endl;
      return false;
  }

  std::filesystem::path output_path = temp_dir / "cjsh";
  std::filesystem::rename(asset_path, output_path);

  try {
      std::filesystem::permissions(output_path, 
          std::filesystem::perms::owner_read | 
          std::filesystem::perms::owner_write | 
          std::filesystem::perms::owner_exec |
          std::filesystem::perms::group_read |
          std::filesystem::perms::group_exec |
          std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec);
  } catch (const std::exception& e) {
      std::cerr << "Error setting file permissions: " << e.what() << std::endl;
  }

  bool update_success = false;
  std::cout << "Administrator privileges required to install the update." << std::endl;
  std::cout << "Please enter your password if prompted." << std::endl;

  std::string sudo_command = "sudo cp " + output_path.string() + " " + cjsh_filesystem::g_cjsh_path.string();
  FILE* sudo_pipe = popen(sudo_command.c_str(), "r");
  int sudo_result = sudo_pipe ? pclose(sudo_pipe) : -1;
  
  if (sudo_result != 0) {
      std::cerr << "Error executing sudo command." << std::endl;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  if (std::filesystem::exists(cjsh_filesystem::g_cjsh_path)) {
      auto new_file_size = std::filesystem::file_size(output_path);
      auto dest_file_size = std::filesystem::file_size(cjsh_filesystem::g_cjsh_path);
      
      if (new_file_size == dest_file_size) {
          std::string sudo_chmod_command = "sudo chmod 755 " + cjsh_filesystem::g_cjsh_path.string();
          FILE* chmod_pipe = popen(sudo_chmod_command.c_str(), "r");
          pclose(chmod_pipe);
          
          update_success = true;
          std::cout << "Update installed successfully with administrator privileges." << std::endl;
          
          std::ofstream changelog((cjsh_filesystem::g_cjsh_data_path / "CHANGELOG.txt").string());
          if (changelog.is_open()) {
              changelog << "Updated to version " << g_cached_version << " on " << get_current_time_string() << std::endl;
              changelog << "See GitHub for full release notes: " << c_github_url << "/releases/tag/v" << g_cached_version << std::endl;
              changelog.close();
          }
      } else {
          std::cout << "Error: The file was not properly installed (size mismatch)." << std::endl;
      }
  } else {
      std::cout << "Error: Installation failed - destination file doesn't exist." << std::endl;
  }

  if (!update_success) {
      std::cout << "Update installation failed. You can manually install the update by running:" << std::endl;
      std::cout << "sudo cp " << output_path.string() << " " << cjsh_filesystem::g_cjsh_path.string() << std::endl;
      std::cout << "sudo chmod 755 " << cjsh_filesystem::g_cjsh_path.string() << std::endl;
      std::cout << "Please ensure you have the necessary permissions." << std::endl;
  }

  try {
      std::filesystem::remove_all(temp_dir);
  } catch (const std::exception& e) {
      std::cerr << "Error cleaning up temporary files: " << e.what() << std::endl;
  }

  if (std::filesystem::exists(cjsh_filesystem::g_cjsh_update_cache_path)) {
      try {
          std::filesystem::remove(cjsh_filesystem::g_cjsh_update_cache_path);
          if (g_debug_mode) {
              std::cout << "Removed old update cache file: " << cjsh_filesystem::g_cjsh_update_cache_path << std::endl;
          }
      } catch (const std::exception& e) {
          std::cerr << "Error removing update cache: " << e.what() << std::endl;
      }
  }

  return update_success;
}

bool execute_update_if_available(bool update_available) {
  if (!update_available) {
    std::cout << "\nYou are up to date!." << std::endl;
    return false;
  }

  
  std::cout << "\nAn update is available. Would you like to download it? (Y/N): ";
  char response;
  std::cin >> response;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  
  if (response != 'Y' && response != 'y') return false;
  
  save_update_cache(false, g_cached_version);
  
  if (!download_latest_release()) {
      std::cout << "Failed to download or install the update. Please try again later or manually update." << std::endl;
      std::cout << "You can download the latest version from: " << c_github_url << "/releases/latest" << std::endl;
      save_update_cache(true, g_cached_version);
      return false;
  }
  
  std::cout << "Update installed successfully! Would you like to restart now? (Y/N): ";
  std::cin >> response;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  
  if (response == 'Y' || response == 'y') {
      std::cout << "Restarting application..." << std::endl;
      
      if (g_shell->get_login_mode()) {
          g_shell->restore_terminal_state();
      }
      
      delete g_shell;
      delete g_ai;
      delete g_theme;
      delete g_plugin;
      
      execl(cjsh_filesystem::g_cjsh_path.c_str(), cjsh_filesystem::g_cjsh_path.c_str(), NULL);
      
      std::cerr << "Failed to restart. Please restart the application manually." << std::endl;
      exit(0);
  } else {
      std::cout << "Please restart the application to use the new version." << std::endl;
  }
  
  return true;
}

void display_changelog(const std::string& changelog_path) {
  std::ifstream file(changelog_path);
  if (!file.is_open()) {
      return;
  }
  
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();
  
  std::cout << "\n===== CHANGELOG =====\n" << content << "\n=====================\n" << std::endl;
}

void startup_update_process() {
  g_first_boot = is_first_boot();
  
  if (g_first_boot) {
    std::cout << "\n" << get_colorized_splash() << std::endl;
    std::cout << "Welcome to CJ's Shell!" << std::endl;
    std::cout << "Type 'help' for a list of commands or 'exit' to quit." << std::endl;
    mark_first_boot_complete();
    g_title_line = false;
  }
  std::string changelog_path = (cjsh_filesystem::g_cjsh_data_path / "CHANGELOG.txt").string();
  if (std::filesystem::exists(changelog_path)) {
    display_changelog(changelog_path);
    std::string saved_changelog_path = (cjsh_filesystem::g_cjsh_data_path / "latest_changelog.txt").string();
    try {
      std::filesystem::rename(changelog_path, saved_changelog_path);
      g_last_updated = get_current_time_string();
    } catch (const std::exception& e) {
      std::cerr << "Error handling changelog: " << e.what() << std::endl;
    }
  } else {
    if (g_check_updates) {
      bool update_available = false;
      
      if (load_update_cache()) {
        if (should_check_for_updates()) {
          update_available = check_for_update();
          save_update_cache(update_available, g_cached_version);
        } else if (g_cached_update) {
          update_available = true;
          if (!g_silent_update_check) {
            std::cout << "\nUpdate available: " << g_cached_version << " (cached)" << std::endl;
          }
        }
      } else {
        save_update_cache(update_available, g_cached_version);
      }
      
      if (update_available) {
        execute_update_if_available(update_available);
      } else if (!g_silent_update_check) {
        std::cout << " You are up to date!" << std::endl;
      }
    }
  }
}

std::string get_colorized_splash() {
  std::vector<std::string> splash_lines = {
      "   ______       __   _____    __  __",
      "  / ____/      / /  / ___/   / / / /",
      " / /      __  / /   \\__ \\   / /_/ / ",
      "/ /___   / /_/ /   ___/ /  / __  /  ",
      "\\____/   \\____/   /____/  /_/ /_/   ",
      "  CJ's Shell v" + c_version
  };
  
  std::string colorized_splash;
  
  // If terminal doesn't support colors, return plain text
  if (colors::g_color_capability == colors::ColorCapability::NO_COLOR) {
    for (const auto& line : splash_lines) {
      colorized_splash += line + "\n";
    }
    return colorized_splash;
  }
  
  // Define gradient start and end colors for each line
  std::vector<std::pair<colors::RGB, colors::RGB>> line_gradients = {
      {colors::RGB(255, 0, 127), colors::RGB(0, 255, 255)},    // Pink to Cyan
      {colors::RGB(0, 255, 255), colors::RGB(255, 0, 255)},    // Cyan to Magenta
      {colors::RGB(255, 0, 255), colors::RGB(255, 255, 0)},    // Magenta to Yellow
      {colors::RGB(255, 255, 0), colors::RGB(0, 127, 255)},    // Yellow to Azure
      {colors::RGB(0, 127, 255), colors::RGB(255, 0, 127)},    // Azure to Pink
      {colors::RGB(255, 128, 0), colors::RGB(0, 255, 128)}     // Orange to Spring Green
  };
  
  for (size_t i = 0; i < splash_lines.size(); i++) {
      colorized_splash += colors::gradient_text(splash_lines[i], 
                                              line_gradients[i].first, 
                                              line_gradients[i].second) + "\n";
  }
  
  return colorized_splash;
}

bool is_first_boot() {
  std::filesystem::path first_boot_flag = cjsh_filesystem::g_cjsh_data_path / ".first_boot_complete";
  return !std::filesystem::exists(first_boot_flag);
}

void mark_first_boot_complete() {
  std::filesystem::path first_boot_flag = cjsh_filesystem::g_cjsh_data_path / ".first_boot_complete";
  std::ofstream flag_file(first_boot_flag);
  flag_file.close();
}
