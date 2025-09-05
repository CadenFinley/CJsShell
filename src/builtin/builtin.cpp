#include "builtin.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iomanip>

#include "ai_command.h"
#include "aihelp_command.h"
#include "alias_command.h"
#include "approot_command.h"
#include "cd_command.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "eval_command.h"
#include "exit_command.h"
#include "export_command.h"
#include "help_command.h"
#include "history_command.h"
#include "ls_command.h"
#include "plugin_command.h"
#include "restart_command.h"
#include "theme_command.h"
#include "uninstall_command.h"
#include "user_command.h"
#include "version_command.h"
#include "prompt_test_command.h"

#define PRINT_ERROR(MSG)                             \
  do {                                               \
    last_terminal_output_error = (MSG);              \
    std::cerr << last_terminal_output_error << '\n'; \
  } while (0)

Built_ins::Built_ins()
      : builtins{
            {"cd",
             [this](const std::vector<std::string>& args) {
              return ::change_directory(args.size() > 1 ? args[1] : current_directory, current_directory, previous_directory, last_terminal_output_error);
             }},
            {"ls",
             [](const std::vector<std::string>& args) {
               return ::ls_command(args);
             }},
            {"alias",
             [this](const std::vector<std::string>& args) {
               return ::alias_command(args, shell);
             }},
            {"export",
             [this](const std::vector<std::string>& args) {
               return ::export_command(args, shell);
             }},
            {"unalias",
             [this](const std::vector<std::string>& args) {
               return ::unalias_command(args, shell);
             }},
            {"unset",
             [this](const std::vector<std::string>& args) {
               return ::unset_command(args, shell);
             }},
            {"ai",
             [this](const std::vector<std::string>& args) {
               return ::ai_command(args, this);
             }},
            {"user",
             [](const std::vector<std::string>& args) {
               return ::user_command(args);
             }},
            {"theme",
             [](const std::vector<std::string>& args) {
               return ::theme_command(args);
             }},
            {"plugin",
             [](const std::vector<std::string>& args) {
               return ::plugin_command(args);
             }},
            {"help",
             [](const std::vector<std::string>&) {
               return ::help_command();
             }},
            {"approot",
             [this](const std::vector<std::string>&) {
               return ::change_to_approot(current_directory, previous_directory, last_terminal_output_error);
             }},
            {"aihelp",
             [](const std::vector<std::string>& args) {
               return ::aihelp_command(args);
             }},
            {"version",
             [](const std::vector<std::string>& args) {
               return ::version_command(args);
             }},
            {"uninstall",
             [](const std::vector<std::string>&) {
               return ::uninstall_command();
             }},
            {"restart",
             [](const std::vector<std::string>& args) {
               return ::restart_command(args);
             }},
            {"eval",
             [this](const std::vector<std::string>& args) {
               return ::eval_command(args, shell);
             }},
            {"history",
             [](const std::vector<std::string>& args) {
               return ::history_command(args);
             }},
            {"exit",
             [](const std::vector<std::string>& args) {
               return ::exit_command(args);
             }},
            {"quit",
             [](const std::vector<std::string>& args) {
               return ::exit_command(args);
             }},
            {"terminal",
             [this](const std::vector<std::string>& args) {
              (void)args;
               shell->set_menu_active(true);
               return 0;
             }},
            {"prompt_test",
             [](const std::vector<std::string>& args) {
               extern int prompt_test_command(const std::vector<std::string>&);
               return prompt_test_command(args);
             }},
        },
        shell(nullptr) {}

int Built_ins::builtin_command(const std::vector<std::string>& args) {
  if (args.empty()) return 1;

  auto it = builtins.find(args[0]);
  if (it != builtins.end()) {
    if (args[0] == "cd" && args.size() == 1) {
      return ::change_directory("", current_directory, previous_directory,
                                last_terminal_output_error);
    }
    int status = it->second(args);
    return status;
  }
  PRINT_ERROR("cjsh: command not found: " + args[0]);
  return 127;
}

int Built_ins::is_builtin_command(const std::string& cmd) const {
  return !cmd.empty() && builtins.find(cmd) != builtins.end();
}

int Built_ins::do_ai_request(const std::string& prompt) {
  return ::ai_command({"ai", prompt}, this);
}