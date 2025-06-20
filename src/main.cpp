#include "main.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>

#include "../vendor/isocline/include/isocline.h"
#include "cjsh_filesystem.h"
#include "colors.h"
#include "completions.h"
#include "shell.h"
#include "update.h"

// to do
//  spec out shell script interpreter

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
 * 143     - Process terminated (SIGTERM)
 * 255     - Exit status out of range
 */

int main(int argc, char* argv[]) {
  if (!initialize_cjsh_path()) {
    std::cerr << "Warning: Unable to determine the executable path. This "
                 "program may not work correctly."
              << std::endl;
  }

  bool login_mode = false;
  bool interactive_mode = true;
  bool force_interactive = false;
  if (argv && argv[0] && argv[0][0] == '-') {
    login_mode = true;
    if (g_debug_mode)
      std::cerr << "DEBUG: Login mode detected from argv[0]: " << argv[0]
                << std::endl;
  }
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--login" || std::string(argv[i]) == "-l" ||
        std::string(argv[i]) == "-cjsh") {
      login_mode = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Login mode detected from command-line argument: "
                  << argv[i] << std::endl;
    }
    if (std::string(argv[i]) == "--interactive" ||
        std::string(argv[i]) == "-i") {
      force_interactive = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Interactive detected from command-line argument: "
                  << argv[i] << std::endl;
    }
    if (std::string(argv[i]) == "--debug") {
      g_debug_mode = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Debug mode enabled from command-line argument: "
                  << argv[i] << std::endl;
    }
  }

  g_shell = std::make_unique<Shell>(login_mode);

  g_startup_args.clear();
  for (int i = 0; i < argc; i++) {
    g_startup_args.push_back(std::string(argv[i]));
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Starting CJ's Shell version " << c_version
              << " with PID: " << c_pid_str << " with args: " << std::endl;
    for (const auto& arg : g_startup_args) {
      std::cerr << "DEBUG:   " << arg << std::endl;
    }
  }

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
      interactive_mode = false;
    } else if (arg == "-v" || arg == "--version") {
      std::cout << c_version << std::endl;
      interactive_mode = false;
    } else if (arg == "-h" || arg == "--help") {
      g_shell->execute_command("help");
      interactive_mode = false;
    } else if (arg == "--login" || arg == "-l" || arg == "--interactive" ||
               arg == "-i" || arg == "-cjsh") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Recognized immeadiate arguement: " << arg
                  << std::endl;
    } else if (arg == "--debug") {
      g_debug_mode = true;
      if (g_debug_mode)
        std::cerr << "DEBUG: Recognized immeadiate arguement: " << arg
                  << std::endl;
    } else if (arg == "--set-as-shell") {
      std::cout
          << "Warning: cjsh is not a POSIX compliant shell. \nSimilar to FISH, "
             "missuse of cjsh or incorrectly settingcjsh as your login shell "
             "can \nhave adverse effects and there is no warranty."
          << std::endl;
      std::cout << "To set cjsh as your default shell you must run these two "
                   "commands:"
                << std::endl;
      std::cout << "To add cjsh to the list of shells:" << std::endl;
      std::cout << "sudo sh -c \"echo " << cjsh_filesystem::g_cjsh_path
                << " >> /etc/shells\"" << std::endl;
      std::cout << "To set CJ's Shell as your default shell:" << std::endl;
      std::cout << "sudo chsh -s " << cjsh_filesystem::g_cjsh_path << std::endl;
      std::cout
          << "Would you like to automatically run these commands? (y/n): ";
      std::string response;
      std::getline(std::cin, response);
      if (response == "y" || response == "Y") {
        std::string command = "sudo sh -c \"echo " +
                              cjsh_filesystem::g_cjsh_path.string() +
                              " >> /etc/shells\"";
        int result = g_shell->execute_command(command);
        if (result != 0) {
          std::cerr << "Error: Failed to add cjsh to /etc/shells." << std::endl;
        } else {
          std::cout << "cjsh added to /etc/shells successfully." << std::endl;
        }
        command = "sudo chsh -s " + cjsh_filesystem::g_cjsh_path.string();
        result = g_shell->execute_command(command);
        if (result == -1) {
          std::cerr << "Error: Failed to set cjsh as default shell."
                    << std::endl;
        } else {
          std::cout << "cjsh set as default shell successfully." << std::endl;
        }
      } else {
        std::cout << "cjsh will not be set as your default shell." << std::endl;
      }
      interactive_mode = false;
    } else if (arg == "--update") {
      execute_update_if_available(check_for_update());
      interactive_mode = false;
    } else if (arg == "--silent-updates") {
      g_silent_update_check = true;
    } else if (arg == "--no-plugins") {
      l_plugins_enabled = false;
    } else if (arg == "--no-themes") {
      l_themes_enabled = false;
    } else if (arg == "--no-ai") {
      l_ai_enabled = false;
    } else if (arg == "--no-colors") {
      l_colors_enabled = false;
    } else if (arg == "--no-update") {
      g_check_updates = false;
    } else if (arg == "--check-update") {
      g_check_updates = true;
    } else if (arg == "--no-titleline") {
      g_title_line = false;
    } else if (arg == "--no-source") {
      source_enabled = false;
    } else if (arg.length() > 0 && arg[0] == '-') {
      std::cerr << "Warning: Unknown startup argument: " << arg << std::endl;
      g_shell.reset();
      return 127;
    }
  }

  if (l_execute_command) {
    int exit_code = g_shell->execute_command(l_cmd_to_execute);
    if ((!interactive_mode && !force_interactive) || exit_code != 0) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Exiting after executing command: "
                  << l_cmd_to_execute << std::endl;
      g_shell.reset();
      return exit_code;
    }
  }

  if (!interactive_mode && !force_interactive) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Interactive mode not enabled" << std::endl;
    g_shell.reset();
    return 0;
  }

  g_shell->set_interactive_mode(true);
  if (!init_interactive_filesystem()) {
    std::cerr << "Error: Failed to initialize or verify file system or files "
                 "within the file system."
              << std::endl;
    g_shell.reset();
    return 1;
  }

  setup_environment_variables();

  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing colors with enabled=" << l_colors_enabled
              << std::endl;
  colors::initialize_color_support(l_colors_enabled);

  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing plugin system with enabled="
              << l_plugins_enabled << std::endl;
  std::unique_ptr<Plugin> plugin = std::make_unique<Plugin>(
      cjsh_filesystem::g_cjsh_plugin_path, l_plugins_enabled);
  g_plugin = plugin.get();

  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing theme system with enabled="
              << l_themes_enabled << std::endl;
  std::unique_ptr<Theme> theme = std::make_unique<Theme>(
      cjsh_filesystem::g_cjsh_theme_path, l_themes_enabled);
  g_theme = theme.get();

  std::string api_key = "";
  const char* env_key = getenv("OPENAI_API_KEY");
  if (env_key) {
    api_key = env_key;
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing AI with enabled=" << l_ai_enabled
              << std::endl;
  std::unique_ptr<Ai> ai = std::make_unique<Ai>(
      api_key, std::string("chat"),
      std::string(
          "You are an AI personal assistant within a users login shell."),
      std::vector<std::string>{}, cjsh_filesystem::g_cjsh_data_path,
      l_ai_enabled);
  g_ai = ai.get();

  if (source_enabled) {
    if (g_debug_mode) std::cerr << "DEBUG: Processing source file" << std::endl;
    process_source_file();
  }

  g_startup_active = false;
  if (!g_exit_flag) {
    startup_update_process();
    if (g_title_line) {
      std::cout << title_line << std::endl;
      std::cout << created_line << std::endl;
    }
    std::atomic<bool> watchdog_should_exit{false};
    std::thread watchdog_thread([&watchdog_should_exit]() {
      while (!watchdog_should_exit && !g_exit_flag) {
        if (!is_parent_process_alive()) {
          std::cerr << "Parent process terminated, shutting down..."
                    << std::endl;
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

    g_shell->process_pending_signals();

    update_terminal_title();

    std::string prompt;
    if (g_shell->get_menu_active()) {
      prompt = g_shell->get_prompt();
    } else {
      prompt = g_shell->get_ai_prompt();
    }

    if (g_theme->uses_newline()) {
      if (write(STDOUT_FILENO, "", 0) < 0) {
        if (errno == EIO || errno == EPIPE) {
          if (g_debug_mode)
            std::cerr << "DEBUG: Terminal disconnected (EIO/EPIPE on stdout)"
                      << std::endl;
          g_exit_flag = true;
          break;
        }
      }
      std::cout << prompt << std::endl;
      prompt = g_shell->get_newline_prompt();
    }

    if (!isatty(STDIN_FILENO)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Terminal disconnected (stdin no longer a TTY)"
                  << std::endl;
      g_exit_flag = true;
      break;
    }

    char* input = ic_readline(prompt.c_str());
    if (input != nullptr) {
      std::string command(input);
      if (g_debug_mode)
        std::cerr << "DEBUG: User input: " << command << std::endl;
      ic_free(input);
      if (!command.empty()) {
        notify_plugins("main_process_command_processed", command);
        update_completion_frequency(command);
        {
          std::string status_str =
              std::to_string(g_shell->execute_command(command));
          if (g_debug_mode)
            std::cerr << "DEBUG: Command exit status: " << status_str
                      << std::endl;
          if (status_str != "127") {
            if (g_debug_mode)
              std::cerr << "DEBUG: Adding command to history: " << command
                        << std::endl;
            ic_history_add(command.c_str());
          }
          setenv("STATUS", status_str.c_str(), 1);
        }
        update_terminal_title();
      }
      if (g_exit_flag) {
        break;
      }
    } else {
      continue;
    }
    notify_plugins("main_process_end", c_pid_str);
    if (g_exit_flag) {
      break;
    }
  }
}

void notify_plugins(std::string trigger, std::string data) {
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
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting up environment variables" << std::endl;

  uid_t uid = getuid();
  struct passwd* pw = getpwuid(uid);

  if (pw != nullptr) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Setting USER=" << pw->pw_name << std::endl;
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);

    const char* path_env = getenv("PATH");
    if (!path_env || path_env[0] == '\0') {
      if (g_debug_mode) std::cerr << "DEBUG: Setting default PATH" << std::endl;
      setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Setting HOSTNAME=" << hostname << std::endl;
      setenv("HOSTNAME", hostname, 1);
    }

    setenv("PWD", std::filesystem::current_path().string().c_str(), 1);
    setenv("SHELL", cjsh_filesystem::g_cjsh_path.c_str(), 1);
    setenv("IFS", " \t\n", 1);

    const char* lang_env = getenv("LANG");
    if (!lang_env || lang_env[0] == '\0') {
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
    std::string status_str = std::to_string(0);
    setenv("STATUS", status_str.c_str(), 1);
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

bool init_interactive_filesystem() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing interactive filesystem" << std::endl;
  std::string current_path = std::filesystem::current_path().string();
  if (g_debug_mode)
    std::cerr << "DEBUG: Current path: " << current_path << std::endl;
  setenv("PWD", current_path.c_str(), 1);
  try {
    if (!std::filesystem::exists(cjsh_filesystem::g_user_home_path)) {
      std::cerr << "cjsh: the users home path could not be determined."
                << std::endl;
      return false;
    }
    initialize_cjsh_directories();
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_history_path)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Creating history file" << std::endl;
      std::ofstream history_file(cjsh_filesystem::g_cjsh_history_path);
      history_file.close();
    }
    if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path)) {
      if (g_debug_mode) std::cerr << "DEBUG: Creating source file" << std::endl;
      create_source_file();
    }

    if (cjsh_filesystem::should_refresh_executable_cache()) {
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

static void capture_profile_env(const std::string& profile_path) {
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    perror("pipe failed");
    return;
  }
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    close(pipefd[0]);
    close(pipefd[1]);
    return;
  }
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    execlp("sh", "sh", "-c",
           (std::string(". \"") + profile_path + "\"; env -0").c_str(),
           (char*)NULL);
    _exit(1);
  }
  close(pipefd[1]);
  std::string data;
  char buf[4096];
  ssize_t n;
  while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) data.append(buf, n);
  close(pipefd[0]);
  waitpid(pid, nullptr, 0);
  for (size_t start = 0; start < data.size();) {
    auto pos = data.find('\0', start);
    if (pos == std::string::npos) break;
    std::string entry = data.substr(start, pos - start);
    auto eq = entry.find('=');
    if (eq != std::string::npos)
      ::setenv(entry.substr(0, eq).c_str(), entry.substr(eq + 1).c_str(), 1);
    start = pos + 1;
  }
}

void process_profile_file() {
  if (g_debug_mode) std::cerr << "DEBUG: Processing profile files" << std::endl;
  std::filesystem::path universal_profile = "/etc/profile";
  if (std::filesystem::exists(universal_profile)) {
    capture_profile_env(universal_profile.string());
  }
  std::filesystem::path user_profile =
      cjsh_filesystem::g_user_home_path / ".profile";
  if (std::filesystem::exists(user_profile)) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Found user profile: " << user_profile.string()
                << std::endl;
    capture_profile_env(user_profile.string());
  }
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_profile_path)) {
    create_profile_file();
    return;
  }
  g_shell->execute_command("source " +
                           cjsh_filesystem::g_cjsh_profile_path.string());
}

void process_source_file() {
  if (!std::filesystem::exists(cjsh_filesystem::g_cjsh_source_path)) {
    create_source_file();
    return;
  }
  g_shell->execute_command("source " +
                           cjsh_filesystem::g_cjsh_source_path.string());
}

void create_profile_file() {
  std::ofstream profile_file(cjsh_filesystem::g_cjsh_profile_path);
  if (profile_file.is_open()) {
    profile_file << "# cjsh Configuration File\n";
    profile_file << "# this file is sourced when the shell starts in login "
                    "mode and is sourced after /etc/profile and ~/.profile\n";
    profile_file << "# this file is the only one that is capable of handling "
                    "startup args";
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