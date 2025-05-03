#include "main.h"
#include "../vendor/isocline/include/isocline.h"
#include "cjsh_filesystem.h"
#include <signal.h>
#include "colors.h"
#include <filesystem>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <errno.h>
#include "update.h"

int main(int argc, char *argv[]) {
  if (g_debug_mode) std::cerr << "DEBUG: main() starting with " << argc << " arguments" << std::endl;

  // verify installation
  if (!initialize_cjsh_path()){
    std::cerr << "Warning: Unable to determine the executable path. This program may not work correctly." << std::endl;
  }

  // login mode and login commands detection: -cjsh, --login, -l
  bool login_mode = false;
  if (argv && argv[0] && argv[0][0] == '-') {
    login_mode = true;
    if (g_debug_mode) std::cerr << "DEBUG: Login mode detected from argv[0]: " << argv[0] << std::endl;
  }
  
  // Additional check for --login or -l arguments
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--login" || std::string(argv[i]) == "-l") {
      login_mode = true;
      if (g_debug_mode) std::cerr << "DEBUG: Login mode detected from command-line argument: " << argv[i] << std::endl;
      break;
    }
  }
  
  // this handles the prompting and executing of commands and signal handling
  g_shell = new Shell(login_mode);

  g_startup_args.clear();
  for (int i = 0; i < argc; i++) {
    g_startup_args.push_back(std::string(argv[i]));
  }
  
  // Initialize login environment if necessary
  if (g_shell->get_login_mode()) {
    if (g_debug_mode) std::cerr << "DEBUG: Initializing login environment" << std::endl;
    if (!init_login_filesystem()) {
      std::cerr << "Error: Failed to initialize or verify file system or files within the file system." << std::endl;
      if (g_shell) {
        delete g_shell;
        g_shell = nullptr;
      }
      return 1;
    }
    process_profile_file();
    initialize_login_environment();
    setup_environment_variables();
    g_shell->setup_job_control();
  }

  if (argv[0]) {
    if (g_debug_mode) std::cerr << "DEBUG: Setting $0=" << argv[0] << std::endl;
    setenv("0", argv[0], 1);
  } else {
    if (g_debug_mode) std::cerr << "DEBUG: Setting $0=unknown" << std::endl;
    setenv("0", "cjsh", 1);
  }
  
  // check for non interactive command line arguments
  bool l_execute_command = false;
  std::string l_cmd_to_execute = "";
  bool l_plugins_enabled = true;
  bool l_themes_enabled = true;
  bool l_ai_enabled = true;
  bool l_colors_enabled = true;
  bool source_enabled = true;
  
  for (size_t i = 1; i < g_startup_args.size(); i++) {
    std::string arg = g_startup_args[i];
    if (arg == "-c" || arg == "--command") {
      if (i + 1 < g_startup_args.size()) {
        l_execute_command = true;
        l_cmd_to_execute = g_startup_args[i + 1];
        i++;
      }
    }
    else if (arg == "-v" || arg == "--version") {
      std::cout << c_version << std::endl;
      if(g_shell) {
        delete g_shell;
        g_shell = nullptr;
      }
      return 0;
    }
    else if (arg == "-h" || arg == "--help") {
      g_shell ->execute_command("help");
      if(g_shell) {
        delete g_shell;
        g_shell = nullptr;
      }
      return 0;
    }
    else if (arg == "--login" || arg == "-l") {
      // simpy to avoid command not found error
      if (g_debug_mode) std::cerr << "DEBUG: Recognized login argument: " << arg << std::endl;
    }
    else if (arg == "--set-as-shell") {
      std::cout << "Setting CJ's Shell as the default shell..." << std::endl;
      std::cerr << "Please run the following command to set CJ's Shell as your default shell:\n";
      std::cerr << "chsh -s " << cjsh_filesystem::g_cjsh_path << std::endl;
      if(g_shell) {
        delete g_shell;
        g_shell = nullptr;
      }
      return 0;
    }
    else if (arg == "--update") {
      execute_update_if_available(check_for_update());
      if(g_shell) {
        delete g_shell;
        g_shell = nullptr;
      }
      return 0;
    }
    else if (arg == "--silent-updates") {
      g_silent_update_check = true;
    }
    else if (arg == "--no-plugins") {
      l_plugins_enabled = false;
    }
    else if (arg == "--no-themes") {
      l_themes_enabled = false;
    }
    else if (arg == "--no-ai") {
      l_ai_enabled = false;
    }
    else if (arg == "--no-colors") {
      l_colors_enabled = false;
    }
    else if (arg == "--splash") {
      colors::initialize_color_support(l_colors_enabled);
      std::cout << get_colorized_splash() << std::endl;
      if(g_shell) {
        delete g_shell;
        g_shell = nullptr;
      }
      return 0;
    }
    else if (arg == "--no-update") {
      g_check_updates = false;
    }
    else if (arg == "-d" || arg == "--debug") {
      g_debug_mode = true;
    }
    else if (arg == "--check-update") {
      g_check_updates = true;
    }
    else if (arg == "--no-titleline") {
      g_title_line = false;
    }
    else if (arg == "--no-source"){
      source_enabled = false;
    }
    else if (arg.length() > 0 && arg[0] == '-') {
      std::cerr << "Warning: Unknown startup argument: " << arg << std::endl;
      if(g_shell) {
        delete g_shell;
        g_shell = nullptr;
      }
      return 1;
    }
  }

  // execute the command passed in the startup arg and exit
  if (l_execute_command) {
    g_shell->execute_command(l_cmd_to_execute);
    if(g_shell) {
      delete g_shell;
      g_shell = nullptr;
    }
    return 0;
  }

  // now we know we are in interactive mode
  g_shell->set_interactive_mode(true);

  // set initial working directory
  std::string current_path = std::filesystem::current_path().string();
  if (g_debug_mode) std::cerr << "DEBUG: Current path: " << current_path << std::endl;
  setenv("PWD", current_path.c_str(), 1);

  if (!init_interactive_filesystem()) {
    std::cerr << "Error: Failed to initialize or verify file system or files within the file system." << std::endl;
    if(g_shell) {
      delete g_shell;
      g_shell = nullptr;
    }
    return 1;
  }

  // ensure bash style envvars are set for interactive shells too
  setup_environment_variables();

  if (g_debug_mode) std::cerr << "DEBUG: Initializing colors with enabled=" << l_colors_enabled << std::endl;
  colors::initialize_color_support(l_colors_enabled);

  if (g_debug_mode) std::cerr << "DEBUG: Initializing plugin system with enabled=" << l_plugins_enabled << std::endl;
  std::unique_ptr<Plugin> plugin = std::make_unique<Plugin>(cjsh_filesystem::g_cjsh_plugin_path, l_plugins_enabled);
  g_plugin = plugin.get();
  
  if (g_debug_mode) std::cerr << "DEBUG: Initializing theme system with enabled=" << l_themes_enabled << std::endl;
  std::unique_ptr<Theme> theme = std::make_unique<Theme>(cjsh_filesystem::g_cjsh_theme_path, l_themes_enabled);
  g_theme = theme.get();
  
  std::string api_key = "";
  const char* env_key = getenv("OPENAI_API_KEY");
  if (env_key) {
    api_key = env_key;
  }
  
  if (g_debug_mode) std::cerr << "DEBUG: Initializing AI with enabled=" << l_ai_enabled << std::endl;
  std::unique_ptr<Ai> ai = std::make_unique<Ai>(
      api_key,
      std::string("chat"),
      std::string("You are an AI personal assistant within a users login shell."),
      std::vector<std::string>{},
      cjsh_filesystem::g_cjsh_data_path,
      l_ai_enabled);
  g_ai = ai.get();
  
  if(source_enabled){
    if (g_debug_mode) std::cerr << "DEBUG: Processing source file" << std::endl;
    process_source_file();
  }

  if(!g_exit_flag) {
    // do update process
    startup_update_process();
    if (g_title_line) {
      std::cout << title_line << std::endl;
      std::cout << created_line  << std::endl;
    }
    std::atomic<bool> watchdog_should_exit{false};
    std::thread watchdog_thread([&watchdog_should_exit]() {
      while (!watchdog_should_exit && !g_exit_flag) {
        if (!is_parent_process_alive()) {
          std::cerr << "Parent process terminated, shutting down..." << std::endl;
          g_exit_flag = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    });

    main_process_loop();
    watchdog_should_exit = true;
    if (watchdog_thread.joinable()) {
      watchdog_thread.join();
    }
  }

  std::cout << "CJ's Shell Exiting..." << std::endl;

  if (g_shell){
    delete g_shell;
    g_shell = nullptr;
  }

  return 0;
}

void update_terminal_title() {
  std::cout << "\033]0;" << g_shell->get_title_prompt() << "\007";
  std::cout.flush();
}

void main_process_loop() {
  if (g_debug_mode) std::cerr << "DEBUG: Entering main process loop" << std::endl;
  notify_plugins("main_process_pre_run", c_pid_str);

  ic_set_prompt_marker("", NULL);
  ic_enable_hint(true);
  ic_set_hint_delay(100);
  ic_enable_completion_preview(true);
  ic_set_history(cjsh_filesystem::g_cjsh_history_path.c_str(), -1);

  while(true) {
    if (g_debug_mode) std::cerr << "DEBUG: Starting new command input cycle" << std::endl;
    notify_plugins("main_process_start", c_pid_str);

    g_shell->process_pending_signals();

    update_terminal_title();
    
    std::string prompt;
    if (g_shell->get_menu_active()) {
      prompt = g_shell->get_prompt();
    } else {
      prompt = g_shell->get_ai_prompt();
    }
    
    if (g_theme -> uses_newline()) {
      if (write(STDOUT_FILENO, "", 0) < 0) {
        if (errno == EIO || errno == EPIPE) {
          if (g_debug_mode) std::cerr << "DEBUG: Terminal disconnected (EIO/EPIPE on stdout)" << std::endl;
          g_exit_flag = true;
          break;
        }
      }
      std::cout << prompt << std::endl;
      prompt = g_shell->get_newline_prompt();
    }

    if (!isatty(STDIN_FILENO)) {
      if (g_debug_mode) std::cerr << "DEBUG: Terminal disconnected (stdin no longer a TTY)" << std::endl;
      g_exit_flag = true;
      break;
    }
    
    char* input = ic_readline(prompt.c_str());
    if (input != nullptr) {
      std::string command(input);
      if(g_debug_mode) std::cerr << "DEBUG: User input: " << command << std::endl;
      ic_free(input);
      if (!command.empty()) {
        notify_plugins("main_process_command_processed", command);
        ic_history_add(command.c_str());
        g_shell->execute_command(command);
        update_terminal_title();
      }
      if (g_exit_flag) {
        break;
      }
    } else {
      if (g_debug_mode) {
        std::cerr << "DEBUG: ic_readline returned nullptr (possible PTY disconnect)" << std::endl;
        if (errno == EIO) std::cerr << "DEBUG: EIO error detected" << std::endl;
        if (errno == EPIPE) std::cerr << "DEBUG: EPIPE error detected" << std::endl;
      }
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
    if (g_debug_mode) std::cerr << "DEBUG: notify_plugins: plugin manager is nullptr" << std::endl;
    return;
  }
  if (g_plugin->get_enabled_plugins().empty()) {
    if (g_debug_mode) std::cerr << "DEBUG: notify_plugins: no enabled plugins" << std::endl;
    return;
  }
  if (g_debug_mode) {
    std::cerr << "DEBUG: Notifying plugins of trigger: " << trigger << " with data: " << data << std::endl;
  }
  g_plugin->trigger_subscribed_global_event(trigger, data);
}

bool init_login_filesystem() {
  if (g_debug_mode) std::cerr << "DEBUG: Initializing login filesystem" << std::endl;
  try {
    if (!std::filesystem::exists(cjsh_filesystem::g_user_home_path)) {
      std::cerr << "cjsh: the users home path could not be determined." << std::endl;
      return false;
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
      if (g_debug_mode) std::cerr << "DEBUG: Creating profile file" << std::endl;
      create_profile_file();
    }
  } catch (const std::exception& e) {
    std::cerr << "cjsh: Failed to initalize the cjsh login filesystem: " << e.what() << std::endl;
    return false;
  }
  return true;
}

void setup_environment_variables() {
  if (g_debug_mode) std::cerr << "DEBUG: Setting up environment variables" << std::endl;
  
  uid_t uid = getuid();
  struct passwd* pw = getpwuid(uid);
  
  if (pw != nullptr) {
    if (g_debug_mode) std::cerr << "DEBUG: Setting USER=" << pw->pw_name << std::endl;
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);
    
    if (getenv("PATH") == nullptr) {
      if (g_debug_mode) std::cerr << "DEBUG: Setting default PATH" << std::endl;
      setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      if (g_debug_mode) std::cerr << "DEBUG: Setting HOSTNAME=" << hostname << std::endl;
      setenv("HOSTNAME", hostname, 1);
    }
    
    setenv("PWD", std::filesystem::current_path().string().c_str(), 1);
    setenv("SHELL", cjsh_filesystem::g_cjsh_path.c_str(), 1);
    setenv("IFS", " \t\n", 1);
    if (getenv("LANG") == nullptr) {
      setenv("LANG", "en_US.UTF-8", 1);
    }
    
    if (getenv("PAGER") == nullptr) {
      setenv("PAGER", "less", 1);
    }

    if (getenv("TMPDIR") == nullptr) {
      setenv("TMPDIR", "/tmp", 1);
    }

    int shlvl = 1;
    if (const char* current_shlvl = getenv("SHLVL")) {
      try {
        shlvl = std::stoi(current_shlvl) + 1;
      } catch (...) {
        shlvl = 1;
      }
    }
    setenv("SHLVL", std::to_string(shlvl).c_str(), 1);
    setenv("_", cjsh_filesystem::g_cjsh_path.c_str(), 1);
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
  
  struct sigaction sa_pipe;
  sa_pipe.sa_handler = SIG_DFL;
  sigemptyset(&sa_pipe.sa_mask);
  sa_pipe.sa_flags = 0;
  sigaction(SIGPIPE, &sa_pipe, nullptr);
}

bool init_interactive_filesystem() {
  if (g_debug_mode) std::cerr << "DEBUG: Initializing interactive filesystem" << std::endl;
  try {
    if (!std::filesystem::exists(cjsh_filesystem::g_user_home_path)) {
      std::cerr << "cjsh: the users home path could not be determined." << std::endl;
      return false;
    }
    initialize_cjsh_directories();
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_history_path)) {
      if (g_debug_mode) std::cerr << "DEBUG: Creating history file" << std::endl;
      std::ofstream history_file(cjsh_filesystem::g_cjsh_history_path);
      history_file.close();
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path)) {
      if (g_debug_mode) std::cerr << "DEBUG: Creating source file" << std::endl;
      create_source_file();
    }
  } catch (const std::exception& e) {
    std::cerr << "cjsh: Failed to initalize the cjsh interactive filesystem: " << e.what() << std::endl;
    return false;
  }
  return true;
}

static void capture_profile_env(const std::string& profile_path) {
    int pipefd[2];
    if (pipe(pipefd) != 0) { perror("pipe failed"); return; }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        close(pipefd[0]); close(pipefd[1]);
        return;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        execlp("sh", "sh", "-c",
               (std::string(". \"")+profile_path+"\"; env -0").c_str(),
               (char*)NULL);
        _exit(1);
    }
    close(pipefd[1]);
    std::string data;
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        data.append(buf, n);
    close(pipefd[0]);
    waitpid(pid, nullptr, 0);
    for (size_t start=0; start < data.size(); ) {
        auto pos = data.find('\0', start);
        if (pos==std::string::npos) break;
        std::string entry = data.substr(start, pos-start);
        auto eq = entry.find('=');
        if (eq!=std::string::npos)
            ::setenv(entry.substr(0,eq).c_str(),
                     entry.substr(eq+1).c_str(), 1);
        start = pos+1;
    }
}

void process_profile_file() {
    if (g_debug_mode) std::cerr << "DEBUG: Processing profile files" << std::endl;
    std::filesystem::path user_profile = cjsh_filesystem::g_user_home_path / ".profile";
    if (std::filesystem::exists(user_profile)) {
      if (g_debug_mode) std::cerr << "DEBUG: Found user profile: " << user_profile.string() << std::endl;
      capture_profile_env(user_profile.string());
    }
    std::filesystem::path universal_profile = "/etc/profile";
    if (std::filesystem::exists(universal_profile)) {
        capture_profile_env(universal_profile.string());
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
        create_profile_file();
        return;
    }
    g_shell->execute_command("source " + cjsh_filesystem::g_cjsh_profile_path.string());
}

void process_source_file() {
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path)) {
    create_source_file();
    return;
  }
  g_shell->execute_command("source " + cjsh_filesystem::g_cjsh_source_path.string());
}

void create_profile_file() {
  std::ofstream profile_file(cjsh_filesystem::g_cjsh_profile_path);
  if (profile_file.is_open()) {
    profile_file << "# cjsh Configuration File\n";
    profile_file << "# this file is sourced when the shell starts in login mode and is sourced after /etc/profile and ~/.profile\n";
    profile_file << "# this file is used to configure the shell PATH and set environment variables.\n";

    profile_file << "# any environment variables should be set without 'export' command\n";
    profile_file << "# example: VARIABLE=value\n\n";

    profile_file << "# this is also where you can add any startup flags you want for cjsh\n";
    profile_file.close();
  } else {
    std::cerr << "cjsh: Failed to create the configuration file." << std::endl;
  }
}

void create_source_file() {
  std::ofstream source_file(cjsh_filesystem::g_cjsh_source_path);
  if (source_file.is_open()) {
    source_file << "# cjsh Source File\n";
    source_file << "# this file is sourced when the shell starts in interactive mode\n";
    source_file << "# this is where your aliases, theme setup, enabled plugins, startup commands, functions etc.";
    source_file << "# are stored.\n";

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
      "  CJ's Shell v" + c_version
  };
  
  std::string colorized_splash;
  if (colors::g_color_capability == colors::ColorCapability::NO_COLOR) {
    for (const auto& line : splash_lines) {
      colorized_splash += line + "\n";
    }
    return colorized_splash;
  }
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
