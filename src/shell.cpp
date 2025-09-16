#include "shell.h"

#include <cstdlib>
#include <set>
#include <sstream>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "job_control.h"
#include "signal_handler.h"
#include "suggestion_utils.h"
#include "trap_command.h"

void Shell::process_pending_signals() {
  if (signal_handler && shell_exec) {
    signal_handler->process_pending_signals(shell_exec.get());
  }
}

Shell::Shell() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Constructing Shell" << std::endl;

  save_terminal_state();

  shell_prompt = std::make_unique<Prompt>();
  shell_exec = std::make_unique<Exec>();
  signal_handler = std::make_unique<SignalHandler>();

  shell_parser = std::make_unique<Parser>();
  built_ins = std::make_unique<Built_ins>();
  shell_script_interpreter = std::make_unique<ShellScriptInterpreter>();

  if (shell_script_interpreter && shell_parser) {
    shell_script_interpreter->set_parser(shell_parser.get());
    shell_parser->set_shell(this);
  }
  built_ins->set_shell(this);
  built_ins->set_current_directory();

  shell_terminal = STDIN_FILENO;

  JobManager::instance().set_shell(this);
  TrapManager::instance().set_shell(this);

  setup_signal_handlers();
  g_signal_handler = signal_handler.get();
  setup_job_control();
}

Shell::~Shell() {
  if (interactive_mode) {
    std::cerr << "Destroying Shell." << std::endl;
  }

  shell_exec->terminate_all_child_process();
  restore_terminal_state();
}

int Shell::execute(const std::string& script) {
  if (script.empty()) {
    return 0;
  }
  std::string processed_script = script;
  if (!get_menu_active()) {
    if (script == ":") {
      set_menu_active(true);
      return 0;
    } else if (script[0] == ':') {
      processed_script = script.substr(1);
    } else {
      return do_ai_request(script);
    }
  }
  std::vector<std::string> lines;

  lines = shell_parser->parse_into_lines(processed_script);

  if (g_debug_mode) {
    std::cerr << "DEBUG: Executing script with " << lines.size()
              << " lines:" << std::endl;
    for (size_t i = 0; i < lines.size(); i++) {
      std::cerr << "DEBUG:   Line " << (i + 1) << ": " << lines[i] << std::endl;
    }
  }

  if (shell_script_interpreter) {
    int exit_code = shell_script_interpreter->execute_block(lines);
    last_command = processed_script;
    return exit_code;
  } else {
    print_error(ErrorInfo{ErrorType::RUNTIME_ERROR,
                          "",
                          "No script interpreter available",
                          {"Restart cjsh"}});
    return 1;
  }
}

void Shell::setup_signal_handlers() {
  signal_handler->setup_signal_handlers();
}

void Shell::setup_interactive_handlers() {
  signal_handler->setup_interactive_handlers();
}

void Shell::save_terminal_state() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Saving terminal state" << std::endl;

  if (isatty(STDIN_FILENO)) {
    if (tcgetattr(STDIN_FILENO, &shell_tmodes) == 0) {
      terminal_state_saved = true;
    }
  }
}

void Shell::restore_terminal_state() {
  if (interactive_mode) {
    std::cerr << "Restoring terminal state." << std::endl;
  }

  if (terminal_state_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &shell_tmodes);
  }
}

void Shell::setup_job_control() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Setting up job control" << std::endl;

  if (!isatty(STDIN_FILENO)) {
    job_control_enabled = false;
    return;
  }

  shell_pgid = getpid();

  if (setpgid(shell_pgid, shell_pgid) < 0) {
    if (errno != EPERM) {
      print_error({ErrorType::RUNTIME_ERROR,
                   "setpgid",
                   "couldn't put the shell in its own process group: " +
                       std::string(strerror(errno)),
                   {}});
    }
  }

  try {
    shell_terminal = STDIN_FILENO;

    int tpgrp = tcgetpgrp(shell_terminal);
    if (tpgrp != -1) {
      if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        print_error(
            {ErrorType::RUNTIME_ERROR,
             "tcsetpgrp",
             "couldn't grab terminal control: " + std::string(strerror(errno)),
             {}});
      }
    }

    job_control_enabled = true;
  } catch (const std::exception& e) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "",
                 e.what(),
                 {"Check terminal settings", "Restart cjsh"}});
    job_control_enabled = false;
  }
}

int Shell::do_ai_request(const std::string& command) {
  if (command == "exit" || command == "clear" || command == "quit") {
    return execute(command);
  }
  std::string first_word;
  std::istringstream iss(command);
  iss >> first_word;

  if (!first_word.empty()) {
    auto cached_executables = cjsh_filesystem::read_cached_executables();
    std::unordered_set<std::string> available_commands =
        get_available_commands();

    bool is_executable = false;
    for (const auto& exec_path : cached_executables) {
      if (exec_path.filename().string() == first_word) {
        is_executable = true;
        break;
      }
    }

    if (!is_executable &&
        available_commands.find(first_word) != available_commands.end()) {
      is_executable = true;
    }

    if (is_executable) {
      std::cout
          << "It looks like you're trying to run a command '" << first_word
          << "' in AI mode. Did you mean to run it as a shell command? (y/n): ";
      std::string response;
      std::getline(std::cin, response);

      if (!response.empty() && (response[0] == 'y' || response[0] == 'Y')) {
        return execute(command);
      }
    }
  }

  return built_ins->do_ai_request(command);
}

int Shell::execute_command(std::vector<std::string> args,
                           bool run_in_background) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Executing command: '" << args[0] << "'" << std::endl;

  if (args.empty()) {
    return 0;
  }
  if (!shell_exec || !built_ins) {
    g_exit_flag = true;
    print_error({ErrorType::RUNTIME_ERROR,
                 "",
                 "Shell not properly initialized",
                 {"Restart cjsh"}});
    return 1;
  }

  if (args.size() == 1 && shell_parser) {
    std::string var_name, var_value;
    if (shell_parser->is_env_assignment(args[0], var_name, var_value)) {
      setenv(var_name.c_str(), var_value.c_str(), 1);
      return 0;
    }

    if (args[0].find('=') != std::string::npos) {
      return 1;
    }
  }

  if (!args.empty() && built_ins->is_builtin_command(args[0])) {
    int code = built_ins->builtin_command(args);
    last_terminal_output_error = built_ins->get_last_error();

    if (args[0] == "break" || args[0] == "continue" || args[0] == "return") {
      if (g_debug_mode)
        std::cerr << "DEBUG: Detected loop control command: " << args[0]
                  << " with exit code " << code << std::endl;
    }

    return code;
  }

  if (g_plugin) {
    std::vector<std::string> enabled_plugins = g_plugin->get_enabled_plugins();
    if (!args.empty() && !enabled_plugins.empty()) {
      for (const auto& plugin : enabled_plugins) {
        std::vector<std::string> plugin_commands =
            g_plugin->get_plugin_commands(plugin);
        if (std::find(plugin_commands.begin(), plugin_commands.end(),
                      args[0]) != plugin_commands.end()) {
          return g_plugin->handle_plugin_command(plugin, args) ? 0 : 1;
        }
      }
    }
  }

  if (run_in_background) {
    int job_id = shell_exec->execute_command_async(args);
    if (job_id > 0) {
      auto jobs = shell_exec->get_jobs();
      auto it = jobs.find(job_id);
      if (it != jobs.end() && !it->second.pids.empty()) {
        pid_t last_pid = it->second.pids.back();
        setenv("!", std::to_string(last_pid).c_str(), 1);

        JobManager::instance().set_last_background_pid(last_pid);

        if (g_debug_mode) {
          std::cerr << "DEBUG: Background job " << job_id
                    << " started with PID " << last_pid << std::endl;
        }
      }
    }
    last_terminal_output_error = "Background command launched";
    return 0;
  } else {
    shell_exec->execute_command_sync(args);
    last_terminal_output_error = shell_exec->get_error_string();
    int exit_code = shell_exec->get_exit_code();

    // Track successful command usage for better suggestions
    if (exit_code == 0 && !args.empty()) {
      suggestion_utils::update_command_usage_stats(args[0]);
    }

    if (exit_code != 0) {
      // Only print errors for actual system errors, not simple command failures
      ErrorInfo error = shell_exec->get_error();
      if (error.type != ErrorType::RUNTIME_ERROR ||
          error.message.find("command failed with exit code") ==
              std::string::npos) {
        shell_exec->print_last_error();
      }
    }
    return exit_code;
  }
}

std::unordered_set<std::string> Shell::get_available_commands() const {
  std::unordered_set<std::string> cmds;
  if (built_ins) {
    auto b = built_ins->get_builtin_commands();
    cmds.insert(b.begin(), b.end());
  }
  for (const auto& alias : aliases) {
    cmds.insert(alias.first);
  }
  if (g_plugin) {
    auto enabled_plugins = g_plugin->get_enabled_plugins();
    for (const auto& plugin : enabled_plugins) {
      auto plugin_commands = g_plugin->get_plugin_commands(plugin);
      cmds.insert(plugin_commands.begin(), plugin_commands.end());
    }
  }
  return cmds;
}

std::string Shell::get_previous_directory() const {
  return built_ins->get_previous_directory();
}

void Shell::set_positional_parameters(const std::vector<std::string>& params) {
  positional_parameters = params;
}

int Shell::shift_positional_parameters(int count) {
  if (count < 0) {
    return 1;
  }

  if (static_cast<size_t>(count) >= positional_parameters.size()) {
    positional_parameters.clear();
  } else {
    positional_parameters.erase(positional_parameters.begin(),
                                positional_parameters.begin() + count);
  }

  return 0;
}

std::vector<std::string> Shell::get_positional_parameters() const {
  return positional_parameters;
}

size_t Shell::get_positional_parameter_count() const {
  return positional_parameters.size();
}

void Shell::set_shell_option(const std::string& option, bool value) {
  shell_options[option] = value;
  if (g_debug_mode) {
    std::cerr << "DEBUG: Set shell option '" << option << "' to "
              << (value ? "true" : "false") << std::endl;
  }
}

bool Shell::get_shell_option(const std::string& option) const {
  auto it = shell_options.find(option);
  return it != shell_options.end() ? it->second : false;
}

bool Shell::is_errexit_enabled() const {
  return get_shell_option("errexit");
}
