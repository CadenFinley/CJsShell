#include "shell.h"
#include "main.h"
#include "built_ins.h"

Shell::Shell(char *argv[]) {
  shell_prompt = std::make_unique<Prompt>();
  shell_exec = std::make_unique<Exec>();
  shell_parser = new Parser();
  built_ins = new Built_ins();

  built_ins->set_current_directory();

  if (argv && argv[0] && argv[0][0] == '-') {
    login_mode = true;
  } else {
    login_mode = false;
  }
  
  // Initialize shell_terminal
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

  // parse the command
  std::vector<std::string> args = shell_parser->parse_command(command);

  //check for clear and exit early
  if (command == "clear") {
    shell_exec->execute_command_sync(args);
    return;
  }
  if (command == "exit" || command == "quit") {
    g_exit_flag = true;
    return;
  }

  // check if user is in ai mode
  if (!g_menu_terminal) {
    if(args[0] == "terminal") {
      g_menu_terminal = true;
      return;
    }
    if(args[0] == "ai") {
      built_ins->ai_commands(args);
      return;
    }
    built_ins->do_ai_request(command);
  }

  // check if command is a built-in command
  if (built_ins->builtin_command(args)) {
    return;
  }

  //check if command is a plugin command
      // std::vector<std::string> enabledPlugins = pluginManager->getEnabledPlugins();
      //   if (!enabledPlugins.empty()) {
      //     std::queue<std::string> tempQueue;
      //     tempQueue.push(lastCommandParsed);
      //     while (!commandsQueue.empty()) {
      //         tempQueue.push(commandsQueue.front());
      //         commandsQueue.pop();
      //     }
      //     for(const auto& plugin : enabledPlugins){
      //         std::vector<std::string> pluginCommands = pluginManager->getPluginCommands(plugin);
      //         if(std::find(pluginCommands.begin(), pluginCommands.end(), lastCommandParsed) != pluginCommands.end()){
      //             pluginManager->handlePluginCommand(plugin, tempQueue);
      //             return;
      //         }
      //     }
      // }

  // process all other commands
  if (sync) {
    shell_exec->execute_command_sync(args);
  } else {
    shell_exec->execute_command_async(args);
  }
  last_command = command;
  last_terminal_output_error = shell_exec->last_terminal_output_error;
}
