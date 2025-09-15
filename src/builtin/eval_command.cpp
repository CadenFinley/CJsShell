#include "eval_command.h"

#include <iostream>

#include "cjsh.h"

int eval_command(const std::vector<std::string>& args, Shell* shell) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: eval_command called with " << args.size()
              << " arguments" << std::endl;
  }

  if (args.size() < 2) {
    std::cerr << "eval: missing arguments" << std::endl;
    return 1;
  }

  std::string command_to_eval;
  for (size_t i = 1; i < args.size(); ++i) {
    if (i > 1) {
      command_to_eval += " ";
    }

    std::string arg = args[i];

    size_t eq_pos = arg.find('=');
    if (eq_pos != std::string::npos && eq_pos > 0) {
      std::string var_name = arg.substr(0, eq_pos);
      std::string partial_value = arg.substr(eq_pos + 1);

      std::string full_value = partial_value;
      size_t j = i + 1;

      while (j < args.size()) {
        const std::string& next_arg = args[j];

        if (next_arg == ";" || next_arg == "&&" || next_arg == "||" ||
            next_arg == "|" || next_arg == "&" ||

            (next_arg.find('/') == std::string::npos &&
             next_arg.find(':') == std::string::npos && next_arg != "with" &&
             next_arg != "spaces" && next_arg.find("export") == 0)) {
          break;
        }

        full_value += " " + next_arg;
        j++;
      }

      if (full_value.find(" ") != std::string::npos) {
        bool already_quoted =
            (full_value.size() >= 2 &&
             ((full_value.front() == '"' && full_value.back() == '"') ||
              (full_value.front() == '\'' && full_value.back() == '\'')));

        if (!already_quoted) {
          bool has_semicolon =
              (!full_value.empty() && full_value.back() == ';');
          if (has_semicolon) {
            full_value.pop_back();
          }

          command_to_eval += var_name + "=" + "\"" + full_value + "\"";
          if (has_semicolon) {
            command_to_eval += ";";
          }
        } else {
          command_to_eval += var_name + "=" + full_value;
        }
      } else {
        command_to_eval += arg;
      }

      i = j - 1;
    } else {
      command_to_eval += arg;
    }
  }

  if (g_debug_mode) {
    std::cerr << "DEBUG: Evaluating command: " << command_to_eval << std::endl;
  }

  if (shell) {
    int result = shell->execute(command_to_eval);
    if (g_debug_mode) {
      std::cerr << "DEBUG: eval command returned: " << result << std::endl;
    }
    return result;
  } else {
    std::cerr << "eval: shell not initialized" << std::endl;
    return 1;
  }
}
