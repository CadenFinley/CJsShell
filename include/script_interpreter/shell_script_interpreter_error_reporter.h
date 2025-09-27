#pragma once

#include <string>
#include <vector>

#include "shell_script_interpreter.h"

namespace shell_script_interpreter {

class ErrorReporter {
   public:
    static void print_error_report(
        const std::vector<ShellScriptInterpreter::SyntaxError>& errors,
        bool show_suggestions = true, bool show_context = true);

    static void print_runtime_error(const std::string& error_message,
                                    const std::string& context = "",
                                    size_t line_number = 0);

   private:
    static size_t get_terminal_width();
};

}  // namespace shell_script_interpreter