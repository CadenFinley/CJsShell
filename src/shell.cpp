#include "shell.h"
#include "main.h"
#include "built_ins.h"

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
}

Shell::~Shell() {
  delete shell_parser;
  delete built_ins;
}

void Shell::execute_command(std::string command, bool sync) {
  //since this is a custom shell be dont return bool we handle errors and error messages in the command execution process
  if (command.empty()) {
    return;
  }
  if (!shell_exec || !built_ins || !shell_parser) {
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
