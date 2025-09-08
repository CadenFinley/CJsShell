#include "shell.h"

#include <cstdlib>
#include <set>
#include <sstream>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "signal_handler.h"

void Shell::process_pending_signals() {
  if (signal_handler && shell_exec) {
    signal_handler->process_pending_signals(shell_exec.get());
  }
}

Shell::Shell(bool login_mode) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Constructing Shell, login_mode=" << login_mode
              << std::endl;

  shell_prompt = std::make_unique<Prompt>();
  shell_exec = std::make_unique<Exec>();
  signal_handler = std::make_unique<SignalHandler>();
  shell_parser = new Parser();
  built_ins = new Built_ins();

  shell_script_interpreter = new ShellScriptInterpreter();
  // Provide the parser to the script interpreter now that it exists
  if (shell_script_interpreter && shell_parser) {
    shell_script_interpreter->set_parser(shell_parser);
    shell_parser->set_shell(this);  // Set shell reference for positional parameters
  }
  built_ins->set_shell(this);
  built_ins->set_current_directory();
  this->login_mode = login_mode;

  shell_terminal = STDIN_FILENO;

  setup_signal_handlers();
  g_signal_handler = signal_handler.get();
  setup_job_control();
}

Shell::~Shell() {
  if (interactive_mode && g_debug_mode) {
    std::cerr << "Destroying Shell" << std::endl;
  }
  delete shell_parser;
  delete built_ins;
  delete shell_script_interpreter;
  shell_exec->terminate_all_child_process();
  restore_terminal_state();
}

int Shell::execute(const std::string& script) {
  last_command = script;
  std::vector<std::string> lines;

  lines = shell_parser->parse_into_lines(script);

  // print all lines
  if (g_debug_mode) {
    std::cerr << "DEBUG: Executing script with " << lines.size()
              << " lines:" << std::endl;
    for (size_t i = 0; i < lines.size(); i++) {
      std::cerr << "DEBUG:   Line " << (i + 1) << ": " << lines[i] << std::endl;
    }
  }

  // so now we have a series of lines basically a pseudo script
  // so now we need to create a script interpreter and execute the block
  if (shell_script_interpreter) {
    return shell_script_interpreter->execute_block(lines);
  } else {
    std::cerr << "Error: No script interpreter available" << std::endl;
    last_exit_code = 1;
  }

  return last_exit_code;
}

void Shell::setup_signal_handlers() {
  signal_handler->setup_signal_handlers();
  save_terminal_state();
}

void Shell::setup_interactive_handlers() {
  signal_handler->setup_interactive_handlers();
  save_terminal_state();
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
  if (interactive_mode && g_debug_mode) {
    std::cerr << "Restoring terminal state" << std::endl;
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
      perror("Couldn't put the shell in its own process group");
    }
  }

  try {
    shell_terminal = STDIN_FILENO;

    int tpgrp = tcgetpgrp(shell_terminal);
    if (tpgrp != -1) {
      if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        perror("Couldn't grab terminal control");
      }
    }

    if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
      perror("Couldn't get terminal attributes");
    }

    job_control_enabled = true;
  } catch (const std::exception& e) {
    std::cerr << "Error setting up terminal: " << e.what() << std::endl;
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
    last_exit_code = 0;
    return last_exit_code;
  }
  if (!shell_exec || !built_ins) {
    last_exit_code = 1;
    g_exit_flag = true;
    std::cerr << "Error: Shell not properly initialized" << std::endl;
    return last_exit_code;
  }

  // run built in command
  if (!args.empty() && built_ins->is_builtin_command(args[0])) {
    int code = built_ins->builtin_command(args);
    last_terminal_output_error = built_ins->get_last_error();
    last_exit_code = code;
    return last_exit_code;
  }

  // run plugin command
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

  // run shell commands
  if (run_in_background) {
    shell_exec->execute_command_async(args);
    last_terminal_output_error = "Background command launched";
    last_exit_code = 0;
    return last_exit_code;
  } else {
    shell_exec->execute_command_sync(args);
    last_terminal_output_error = shell_exec->get_error();
    last_exit_code = shell_exec->get_exit_code();
    return last_exit_code;
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

// Positional parameters support
void Shell::set_positional_parameters(const std::vector<std::string>& params) {
  positional_parameters = params;
}

int Shell::shift_positional_parameters(int count) {
  if (count < 0) {
    return 1;  // Error: negative shift count
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
