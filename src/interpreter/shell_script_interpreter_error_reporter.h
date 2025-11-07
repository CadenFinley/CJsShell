#pragma once

#include <string>
#include <system_error>
#include <vector>

#include "shell_script_interpreter.h"

namespace shell_script_interpreter {

void print_error_report(const std::vector<ShellScriptInterpreter::SyntaxError>& errors,
                        bool show_suggestions = true, bool show_context = true);

void print_runtime_error(const std::string& error_message, const std::string& context = "",
                         size_t line_number = 0);

void reset_error_count();

int handle_memory_allocation_error(const std::string& text);
int handle_system_error(const std::string& text, const std::system_error& e);
int handle_runtime_error(const std::string& text, const std::runtime_error& e, size_t line_number);
int handle_generic_exception(const std::string& text, const std::exception& e);
int handle_unknown_error(const std::string& text);

bool report_error(const ErrorInfo& error);

}  // namespace shell_script_interpreter