#pragma once

#include <string>
#include <vector>

#include "shell_script_interpreter.h"

namespace shell_script_interpreter {

void print_error_report(const std::vector<ShellScriptInterpreter::SyntaxError>& errors,
                        bool show_suggestions = true, bool show_context = true,
                        int start_error_number = -1);

void print_runtime_error(const std::string& error_message, const std::string& context = "",
                         size_t line_number = 0);

void reset_error_count();

}  // namespace shell_script_interpreter