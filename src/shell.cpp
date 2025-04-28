#include "shell.h"
#include "main.h"
#include "built_ins.h"

// Global pointer to the current Shell instance for signal handlers
Shell* g_shell_instance = nullptr;

// Signal flags
static volatile sig_atomic_t sigint_received = 0;
static volatile sig_atomic_t sigchld_received = 0;
static volatile sig_atomic_t sighup_received = 0;
static volatile sig_atomic_t sigterm_received = 0;

// Signal handler implementation - minimal and async-signal-safe
void shell_signal_handler(int signum, siginfo_t* info, void* context) {
  (void)context; // Unused parameter
  (void)info; // Unused parameter

  switch (signum) {
    case SIGINT: {
      sigint_received = 1;
      // Safe to write a small amount as it's async-signal-safe
      ssize_t bytes_written = write(STDOUT_FILENO, "\n", 1);
      (void)bytes_written; // Prevent unused variable warning
      break;
    }
      
    case SIGCHLD: {
      sigchld_received = 1;
      break;
    }
      
    case SIGHUP: {
      sighup_received = 1;
      _exit(0); // Immediate exit for terminal disconnect
      break;
    }
      
    case SIGTERM: {
      sigterm_received = 1;
      _exit(0); // Immediate exit for termination
      break;
    }
  }
}

// New method to process pending signals from the main loop
void Shell::process_pending_signals() {
  // Handle SIGINT
  if (sigint_received) {
    sigint_received = 0;
    
    // Check if there's a foreground job running and send it SIGINT
    if (shell_exec) {
      auto jobs = shell_exec->get_jobs();
      for (const auto& job_pair : jobs) {
        const auto& job = job_pair.second;
        if (!job.background && !job.completed && !job.stopped) {
          // Send SIGINT to the process group
          if (kill(-job.pgid, SIGINT) < 0) {
            perror("kill (SIGINT) in process_pending_signals");
          }
          break;
        }
      }
    }
    fflush(stdout);
  }
  
  // Handle SIGCHLD
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
  
  // SIGHUP and SIGTERM are handled directly in the signal handler with _exit()
}

Shell::Shell(char *argv[]) {
  shell_prompt = std::make_unique<Prompt>();
  shell_exec = std::make_unique<Exec>();
  shell_parser = new Parser();
  built_ins = new Built_ins();
  built_ins->set_shell(this);
  built_ins->set_current_directory();

  if (argv && argv[0] && argv[0][0] == '-') {
    login_mode = true;
  } else {
    login_mode = false;
  }
  
  shell_terminal = STDIN_FILENO;
  
  // Set global shell instance for signal handlers
  g_shell_instance = this;

  setup_signal_handlers();
}

Shell::~Shell() {
  delete shell_parser;
  delete built_ins;
  restore_terminal_state();
  
  // Unset global shell instance
  if (g_shell_instance == this) {
    g_shell_instance = nullptr;
  }
}

void Shell::setup_signal_handlers() {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sigset_t block_mask;
  sigfillset(&block_mask);

  // Setup SIGHUP handler (terminal disconnect)
  sa.sa_sigaction = shell_signal_handler;
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

void Shell::save_terminal_state() {
  if (isatty(STDIN_FILENO)) {
    if (tcgetattr(STDIN_FILENO, &shell_tmodes) == 0) {
      terminal_state_saved = true;
    }
  }
}

void Shell::restore_terminal_state() {
  if (terminal_state_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &shell_tmodes);
  }
}

void Shell::setup_job_control() {
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
    
    // Get current foreground process group
    int tpgrp = tcgetpgrp(shell_terminal);
    if (tpgrp != -1) {
      // Take control of the terminal
      if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        perror("Couldn't grab terminal control");
      }
    }
    
    // Save terminal attributes
    if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
      perror("Couldn't get terminal attributes");
    }
    
    job_control_enabled = true;
  } catch (const std::exception& e) {
    std::cerr << "Error setting up terminal: " << e.what() << std::endl;
    job_control_enabled = false;
  }
}

// Existing code
void Shell::execute_command(std::string command, bool sync) {
  //since this is a custom shell be dont return bool we handle errors and error messages in the command execution process
  if (command.empty()) {
    return;
  }
  if (!shell_exec || !built_ins || !shell_parser) {
    return;
  }

  // Check for logical operators (&&, ||)
  if (command.find("&&") != std::string::npos || command.find("||") != std::string::npos) {
    std::vector<LogicalCommand> logical_commands = shell_parser->parse_logical_commands(command);
    
    bool prev_success = true; // Assume first command should run
    
    for (size_t i = 0; i < logical_commands.size(); i++) {
      const auto& cmd_segment = logical_commands[i];
      
      // Determine if we should execute this command based on previous result and operator
      bool should_execute = true;
      if (i > 0) {
        const auto& prev_op = logical_commands[i-1].op;
        if (prev_op == "&&") {
          should_execute = prev_success;
        } else if (prev_op == "||") {
          should_execute = !prev_success;
        }
      }
      
      if (should_execute) {
        // Check for built-in commands first
        std::vector<std::string> args = shell_parser->parse_command(cmd_segment.command);
        
        // Special handling for built-in commands, especially 'cd'
        if (!args.empty() && built_ins->is_builtin_command(args[0])) {
          bool result = built_ins->builtin_command(args);
          if (!result) {
            last_terminal_output_error = "Something went wrong with the built-in command";
            prev_success = false;
          } else {
            last_terminal_output_error = "command completed successfully";
            prev_success = true;
          }
        } else {
          // Not a built-in, execute normally
          execute_command(cmd_segment.command, sync);
          
          // Determine success based on error message
          prev_success = (last_terminal_output_error == "command completed successfully" || 
                         last_terminal_output_error == "async command launched");
        }
      }
    }
    
    return;
  }

  // Check for clear and exit early
  if (command == "clear") {
    std::vector<std::string> args = {"clear"};
    shell_exec->execute_command_sync(args);
    return;
  }
  if (command == "exit" || command == "quit") {
    g_exit_flag = true;
    return;
  }

  // Check for pipe symbols to determine if this is a pipeline
  if (command.find('|') != std::string::npos) {
    std::vector<Command> pipeline = shell_parser->parse_pipeline(command);
    shell_exec->execute_pipeline(pipeline);
    last_terminal_output_error = shell_exec->get_error();
    last_command = command;
    return;
  }

  // check if user is in ai mode
  if (!g_menu_terminal) {
    std::vector<std::string> args = shell_parser->parse_command(command);
    if(args[0] == "terminal") {
      g_menu_terminal = true;
      return;
    }
    if(args[0] == "ai") {
      built_ins->ai_commands(args);
      return;
    }
    built_ins->do_ai_request(command);
    return;
  }

  // Parse the command normally
  std::vector<std::string> args = shell_parser->parse_command(command);
  
  // check if command is a built-in command
  if (built_ins->is_builtin_command(args[0])) {
    if(!built_ins->builtin_command(args)){
      last_terminal_output_error = "Something went wrong with the command";
    }
    last_command = command;
    return;
  }

  //check if command is a plugin command
  if (g_plugin) {
    std::vector<std::string> enabled_plugins = g_plugin->get_enabled_plugins();
    if (!enabled_plugins.empty()) {
      for(const auto& plugin : enabled_plugins){
        std::vector<std::string> plugin_commands = g_plugin->get_plugin_commands(plugin);
        if(std::find(plugin_commands.begin(), plugin_commands.end(), args[0]) != plugin_commands.end()){
          g_plugin->handle_plugin_command(plugin, args);
          return;
        }
      }
    }
  }

  // process all other commands
  if (sync) {
    shell_exec->execute_command_sync(args);
    // Only set last_terminal_output_error for synchronous commands
    last_terminal_output_error = shell_exec->get_error();
  } else {
    shell_exec->execute_command_async(args);
    // For async commands, don't try to read the error buffer immediately
    last_terminal_output_error = "async command launched";
  }
  last_command = command;
}
