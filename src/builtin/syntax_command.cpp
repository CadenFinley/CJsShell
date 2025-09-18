#include "syntax_command.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "cjsh.h"
#include "error_out.h"

int syntax_command(const std::vector<std::string>& args, Shell* shell) {
  if (g_debug_mode) {
    std::cerr << "DEBUG: syntax_command called with " << args.size()
              << " arguments" << std::endl;
  }

  if (args.size() < 2) {
    std::cout << "Usage: syntax [options] <script_file>" << std::endl;
    std::cout << "       syntax [options] -c <command_string>" << std::endl;
    std::cout << "Check syntax of shell scripts or commands" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help          Show this help message" << std::endl;
    std::cout << "  -v, --verbose       Show detailed error information"
              << std::endl;
    std::cout << "  -q, --quiet         Only show error count" << std::endl;
    std::cout << "  --no-suggestions    Don't show fix suggestions"
              << std::endl;
    std::cout << "  --no-context        Don't show line context" << std::endl;
    std::cout << "  --comprehensive     Run all validation checks" << std::endl;
    std::cout << "  --semantic          Include semantic analysis" << std::endl;
    std::cout << "  --style             Include style checking" << std::endl;
    std::cout << "  --performance       Include performance analysis"
              << std::endl;
    std::cout << "  --severity LEVEL    Filter by severity "
                 "(info,warning,error,critical)"
              << std::endl;
    std::cout << "  --category CAT      Filter by category "
                 "(syntax,variables,redirection,etc.)"
              << std::endl;
    return 1;
  }

  if (!shell) {
    print_error(
        {ErrorType::RUNTIME_ERROR, "syntax", "shell not initialized", {}});
    return 1;
  }

  bool quiet = false;
  bool show_suggestions = true;
  bool show_context = true;
  bool comprehensive = false;
  bool check_semantics = false;
  bool check_style = false;
  bool check_performance = false;
  std::string severity_filter = "";
  std::string category_filter = "";
  std::string target_file = "";
  std::string command_string = "";
  bool is_command_string = false;

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "-h" || arg == "--help") {
      return 0;
    } else if (arg == "-q" || arg == "--quiet") {
      quiet = true;
    } else if (arg == "--no-suggestions") {
      show_suggestions = false;
    } else if (arg == "--no-context") {
      show_context = false;
    } else if (arg == "--comprehensive") {
      comprehensive = true;
      check_semantics = true;
      check_style = true;
    } else if (arg == "--semantic") {
      check_semantics = true;
    } else if (arg == "--style") {
      check_style = true;
    } else if (arg == "--performance") {
      check_performance = true;
    } else if (arg == "--severity" && i + 1 < args.size()) {
      severity_filter = args[++i];
    } else if (arg == "--category" && i + 1 < args.size()) {
      category_filter = args[++i];
    } else if (arg == "-c") {
      is_command_string = true;

      for (size_t j = i + 1; j < args.size(); ++j) {
        if (j > i + 1)
          command_string += " ";
        command_string += args[j];
      }
      break;
    } else if (!arg.empty() && arg[0] != '-') {
      target_file = arg;
      break;
    }
  }

  std::vector<std::string> lines;

  if (is_command_string) {
    if (command_string.empty()) {
      print_error({ErrorType::INVALID_ARGUMENT,
                   "syntax",
                   "-c option requires a command string",
                   {}});
      return 1;
    }

    auto script_interpreter = shell->get_shell_script_interpreter();
    if (!script_interpreter) {
      print_error({ErrorType::RUNTIME_ERROR,
                   "syntax",
                   "script interpreter not available",
                   {}});
      return 1;
    }

    lines = script_interpreter->parse_into_lines(command_string);
  } else {
    if (target_file.empty()) {
      print_error({ErrorType::INVALID_ARGUMENT,
                   "syntax",
                   "no input file specified",
                   {}});
      return 1;
    }

    std::ifstream file(target_file);
    if (!file.is_open()) {
      print_error({ErrorType::FILE_NOT_FOUND,
                   "syntax",
                   "cannot open file '" + target_file + "'",
                   {}});
      return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    file.close();
  }

  auto script_interpreter = shell->get_shell_script_interpreter();
  if (!script_interpreter) {
    print_error({ErrorType::RUNTIME_ERROR,
                 "syntax",
                 "script interpreter not available",
                 {}});
    return 1;
  }

  std::vector<ShellScriptInterpreter::SyntaxError> errors;

  if (comprehensive) {
    errors = script_interpreter->validate_comprehensive_syntax(
        lines, check_semantics, check_style, check_performance);
  } else {
    errors = script_interpreter->validate_script_syntax(lines);

    if (check_semantics) {
      auto semantic_errors =
          script_interpreter->validate_command_existence(lines);
      errors.insert(errors.end(), semantic_errors.begin(),
                    semantic_errors.end());
    }

    if (check_style) {
      auto style_errors = script_interpreter->check_style_guidelines(lines);
      errors.insert(errors.end(), style_errors.begin(), style_errors.end());
    }
  }

  if (!severity_filter.empty()) {
    ShellScriptInterpreter::ErrorSeverity filter_severity;
    if (severity_filter == "info")
      filter_severity = ShellScriptInterpreter::ErrorSeverity::INFO;
    else if (severity_filter == "warning")
      filter_severity = ShellScriptInterpreter::ErrorSeverity::WARNING;
    else if (severity_filter == "error")
      filter_severity = ShellScriptInterpreter::ErrorSeverity::ERROR;
    else if (severity_filter == "critical")
      filter_severity = ShellScriptInterpreter::ErrorSeverity::CRITICAL;
    else {
      print_error({ErrorType::INVALID_ARGUMENT,
                   "syntax",
                   "unknown severity level '" + severity_filter + "'",
                   {"Valid levels: info, warning, error, critical"}});
      return 1;
    }

    errors.erase(
        std::remove_if(
            errors.begin(), errors.end(),
            [filter_severity](const ShellScriptInterpreter::SyntaxError& err) {
              return err.severity != filter_severity;
            }),
        errors.end());
  }

  if (!category_filter.empty()) {
    ShellScriptInterpreter::ErrorCategory filter_category;
    if (category_filter == "syntax")
      filter_category = ShellScriptInterpreter::ErrorCategory::SYNTAX;
    else if (category_filter == "variables")
      filter_category = ShellScriptInterpreter::ErrorCategory::VARIABLES;
    else if (category_filter == "redirection")
      filter_category = ShellScriptInterpreter::ErrorCategory::REDIRECTION;
    else if (category_filter == "control")
      filter_category = ShellScriptInterpreter::ErrorCategory::CONTROL_FLOW;
    else if (category_filter == "commands")
      filter_category = ShellScriptInterpreter::ErrorCategory::COMMANDS;
    else if (category_filter == "semantics")
      filter_category = ShellScriptInterpreter::ErrorCategory::SEMANTICS;
    else if (category_filter == "style")
      filter_category = ShellScriptInterpreter::ErrorCategory::STYLE;
    else if (category_filter == "performance")
      filter_category = ShellScriptInterpreter::ErrorCategory::PERFORMANCE;
    else {
      print_error({ErrorType::INVALID_ARGUMENT,
                   "syntax",
                   "unknown category '" + category_filter + "'",
                   {"Valid categories: syntax, variables, redirection, "
                    "control, commands, semantics, style, performance"}});
      return 1;
    }

    errors.erase(
        std::remove_if(
            errors.begin(), errors.end(),
            [filter_category](const ShellScriptInterpreter::SyntaxError& err) {
              return err.category != filter_category;
            }),
        errors.end());
  }

  if (quiet) {
    std::cout << errors.size() << std::endl;
  } else {
    script_interpreter->print_error_report(errors, show_suggestions,
                                           show_context);
  }

  return errors.empty() ? 0 : 1;
}
