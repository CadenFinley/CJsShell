#include "shell.h"
#include "main.h"
#include "built_ins.h"
#include <sstream>
#include <cstdlib>   // for setenv

static volatile sig_atomic_t sigint_received = 0;
static volatile sig_atomic_t sigchld_received = 0;
static volatile sig_atomic_t sighup_received = 0;
static volatile sig_atomic_t sigterm_received = 0;

void shell_signal_handler(int signum, siginfo_t* info, void* context) {
  (void)context;
  (void)info;

  if (g_debug_mode) {
    std::cerr << "DEBUG: Signal received: " << signum << std::endl;
  }

  switch (signum) {
    case SIGINT: {
      sigint_received = 1;
      ssize_t bytes_written = write(STDOUT_FILENO, "\n", 1);
      (void)bytes_written;
      if (g_debug_mode) std::cerr << "DEBUG: SIGINT handler executed" << std::endl;
      break;
    }
      
    case SIGCHLD: {
      sigchld_received = 1;
      if (g_debug_mode) std::cerr << "DEBUG: SIGCHLD handler executed" << std::endl;
      break;
    }
      
    case SIGHUP: {
      sighup_received = 1;
      if (g_debug_mode) std::cerr << "DEBUG: SIGHUP handler executed, exiting" << std::endl;
      _exit(0);
      break;
    }
      
    case SIGTERM: {
      sigterm_received = 1;
      if (g_debug_mode) std::cerr << "DEBUG: SIGTERM handler executed, exiting" << std::endl;
      _exit(0);
      break;
    }
  }
}

void Shell::process_pending_signals() {
  if (g_debug_mode && (sigint_received || sigchld_received)) {
    std::cerr << "DEBUG: Processing pending signals: "
              << "SIGINT=" << sigint_received << ", "
              << "SIGCHLD=" << sigchld_received << std::endl;
  }

  if (sigint_received) {
    sigint_received = 0;
    
    if (shell_exec) {
      auto jobs = shell_exec->get_jobs();
      for (const auto& job_pair : jobs) {
        const auto& job = job_pair.second;
        if (!job.background && !job.completed && !job.stopped) {
          if (kill(-job.pgid, SIGINT) < 0) {
            perror("kill (SIGINT) in process_pending_signals");
          }
          break;
        }
      }
    }
    fflush(stdout);
  }
  
  if (sigchld_received) {
    sigchld_received = 0;
    
    if (shell_exec) {
      pid_t pid;
      int status;
      while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        shell_exec->handle_child_signal(pid, status);
      }
    }
  }
}

Shell::Shell(bool login_mode) {
  if (g_debug_mode) std::cerr << "DEBUG: Constructing Shell, login_mode=" << login_mode << std::endl;

  shell_prompt = std::make_unique<Prompt>();
  shell_exec = std::make_unique<Exec>();
  shell_parser = new Parser();
  built_ins = new Built_ins();
  shell_script_interpreter = new ShellScriptInterpreter();
  built_ins->set_shell(this);
  built_ins->set_current_directory();
  this->login_mode = login_mode;

  shell_terminal = STDIN_FILENO;

  setup_signal_handlers();
}

Shell::~Shell() {
  if(interactive_mode) {
    std::cerr << "Destroying Shell" << std::endl;
  }
  delete shell_parser;
  delete built_ins;
  delete shell_script_interpreter;
  shell_exec->terminate_all_child_process();
  if (login_mode) {
    restore_terminal_state();
  }
}

void Shell::setup_signal_handlers() {
  if (g_debug_mode) std::cerr << "DEBUG: Setting up signal handlers" << std::endl;

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sigset_t block_mask;
  sigfillset(&block_mask);

  sa.sa_sigaction = shell_signal_handler;
  sa.sa_mask = block_mask;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGHUP, &sa, nullptr);

  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGTERM, &sa, nullptr);

  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGCHLD, &sa, nullptr);

  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGINT, &sa, nullptr);

  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sa.sa_mask = block_mask;
  sigaction(SIGQUIT, &sa, nullptr);
  sigaction(SIGTSTP, &sa, nullptr);
  sigaction(SIGTTIN, &sa, nullptr);
  sigaction(SIGTTOU, &sa, nullptr);

  save_terminal_state();
}

void Shell::save_terminal_state() {
  if (g_debug_mode) std::cerr << "DEBUG: Saving terminal state" << std::endl;

  if (isatty(STDIN_FILENO)) {
    if (tcgetattr(STDIN_FILENO, &shell_tmodes) == 0) {
      terminal_state_saved = true;
    }
  }
}

void Shell::restore_terminal_state() {
  if(interactive_mode) {
    std::cerr << "Restoring terminal state" << std::endl;
  }

  if (terminal_state_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &shell_tmodes);
  }
}

void Shell::setup_job_control() {
  if (g_debug_mode) std::cerr << "DEBUG: Setting up job control" << std::endl;

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

int Shell::execute_command(std::string command) {
  if (g_debug_mode) std::cerr << "DEBUG: Executing command: '" << command << std::endl;
  if (command.empty()) {
    return 0;
  }
  if (command[0] == '#') {
    return 0;
  }
  if(!shell_exec || !shell_parser || !built_ins || !shell_script_interpreter) {
    return 1;
  }

  while (!command.empty() && std::isspace(command.back())) {
    command.pop_back();
  }
  
  bool run_in_background = false;
  if (!command.empty() && command.back() == '&') {
    run_in_background = true;
    command.pop_back();
    while (!command.empty() && std::isspace(command.back())) {
      command.pop_back();
    }
  }
 
  if (command.empty()) {
    return 0;
  }

  if (command == "clear") {
    std::vector<std::string> clear_args = {"clear"};
    shell_exec->execute_command_sync(clear_args);
    return 0;
  }
  if (command == "exit" || command == "quit") {
    g_exit_flag = true;
    return 0;
  }


  if (menu_active && command.find('\n') != std::string::npos) {
    std::istringstream iss(command);
    std::string line;
    bool prev_success = true;
    int exit_code = 0;
    while (prev_success && std::getline(iss, line)) {
      if (line.empty()) continue;
      exit_code = execute_command(line);
      prev_success = (exit_code == 0);
    }
    return exit_code;
  }

  std::vector<std::string> args = shell_parser->parse_command(command);

  if (!menu_active && args.size() > 0) {
    if (args[0][0] != ':'){
      if(!command.empty()) {
        if(command == "terminal") {
          menu_active = true;
          return 0;
        }
      }
      return built_ins->do_ai_request(command);
    } else {
      args[0].erase(0, 1);
    }
  }

  if (command.find(';') != std::string::npos) {
    std::vector<std::string> cmds = shell_parser->parse_semicolon_commands(command);
    int exit_code = 0;
    for (const auto& cmd : cmds) {
      exit_code = execute_command(cmd);
    }
    return exit_code;
  }
  
  std::string var_name, var_value;
  if (shell_parser->is_env_assignment(command, var_name, var_value)) {
    env_vars[var_name] = var_value;
    shell_parser->set_env_vars(env_vars);
    last_terminal_output_error = "Variable set successfully";
    last_command = command;
    return 0;
  }

  if (!args.empty() && args[0] == "export") {
    if (args.size() > 1) {
      for (size_t i = 1; i < args.size(); i++) {
        std::string var_name, var_value;
        if (shell_parser->is_env_assignment(args[i], var_name, var_value)) {
          env_vars[var_name] = var_value;
          setenv(var_name.c_str(), var_value.c_str(), 1);
        } else if (env_vars.find(args[i]) != env_vars.end()) {
          setenv(args[i].c_str(), env_vars[args[i]].c_str(), 1);
        }
      }
      shell_parser->set_env_vars(env_vars);
      last_terminal_output_error = "Variables exported successfully";
      last_command = command;
      return 0;
    }
  }

  if (command.find("&&") != std::string::npos ||
      command.find("||") != std::string::npos) {
    auto logical_commands = shell_parser->parse_logical_commands(command);
    bool prev_success = true;
    int exit_code = 0;
    for (size_t i = 0; i < logical_commands.size(); ++i) {
      const auto& segment = logical_commands[i];
      if (i > 0) {
        const std::string& prev_op = logical_commands[i-1].op;
        if ((prev_op == "&&" && !prev_success) ||
            (prev_op == "||" &&  prev_success)) {
          break;
        }
      }
      exit_code = execute_command(segment.command);
      prev_success = (exit_code == 0);
    }
    last_command = command;
    last_terminal_output_error = prev_success
      ? "command completed successfully"
      : "command failed";
    return exit_code;
  }

  if (command.find('|') != std::string::npos) {
    std::vector<Command> pipeline = shell_parser->parse_pipeline(command);
    if (run_in_background && !pipeline.empty()) {
      pipeline.back().background = true;
    }
    int result = shell_exec->execute_pipeline(pipeline);
    last_terminal_output_error = shell_exec->get_error();
    last_command = command + (run_in_background ? " &" : "");
    return result;
  }

  if (!args.empty() && built_ins->is_builtin_command(args[0])) {
    int code = built_ins->builtin_command(args);
    last_terminal_output_error = built_ins->get_last_error();
    last_command = command + (run_in_background ? " &" : "");
    return code;
  }

  if (g_plugin) {
    std::vector<std::string> enabled_plugins = g_plugin->get_enabled_plugins();
    if (!args.empty() && !enabled_plugins.empty()) {
      for(const auto& plugin : enabled_plugins){
        std::vector<std::string> plugin_commands = g_plugin->get_plugin_commands(plugin);
        if(std::find(plugin_commands.begin(), plugin_commands.end(), args[0]) != plugin_commands.end()){
          return g_plugin->handle_plugin_command(plugin, args) ? 0 : 1;
        }
      }
    }
  }

  if (args.size() > 0 && args[0] == "source") {
    if (args.size() > 1) {
      if (shell_script_interpreter != nullptr) {
        shell_script_interpreter->execute_script(args[1]);
      } else {
        last_terminal_output_error = "Script interpreter not available";
        return 1;
      }
    } else {
      last_terminal_output_error = "No source file specified";
      return 1;
    }
    return 0;
  }
  if (run_in_background) {
    shell_exec->execute_command_async(args);
    last_terminal_output_error = "Background command launched";
    last_command = command + (run_in_background ? " &" : "");
    return 0;
  } else {
    shell_exec->execute_command_sync(args);
    last_terminal_output_error = shell_exec->get_error();
    last_command = command + (run_in_background ? " &" : "");
    return shell_exec->get_exit_code();
  }
}

std::vector<std::string> Shell::get_available_commands() const {
  std::vector<std::string> cmds;
  if (built_ins) {
    auto b = built_ins->get_builtin_and_alias_names();
    cmds.insert(cmds.end(), b.begin(), b.end());
  }
  return cmds;
}
