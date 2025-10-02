#include "if_command.h"

#include <iostream>
#include "error_out.h"
#include "shell.h"

int if_command(const std::vector<std::string>& args, Shell* shell,
               std::string& last_terminal_output_error) {
    auto record_error = [&](const ErrorInfo& info) {
        last_terminal_output_error = info.message;
        print_error(info);
    };

    if (args.size() < 2) {
        record_error({ErrorType::INVALID_ARGUMENT, "if", "missing arguments", {}});
        return 2;
    }

    std::string full_cmd;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1)
            full_cmd += " ";
        full_cmd += args[i];
    }

    size_t then_pos = full_cmd.find("; then ");
    size_t fi_pos = full_cmd.rfind("; fi");

    if (then_pos == std::string::npos || fi_pos == std::string::npos) {
        record_error(
            {ErrorType::SYNTAX_ERROR, "if", "syntax error: expected '; then' and '; fi'", {}});
        return 2;
    }

    std::string condition = full_cmd.substr(0, then_pos);
    std::string then_cmd = full_cmd.substr(then_pos + 7, fi_pos - (then_pos + 7));

    if (!shell) {
        record_error({ErrorType::RUNTIME_ERROR, "if", "shell context is null", {}});
        return 1;
    }

    int cond_result = shell->execute(condition);

    if (cond_result == 0) {
        return shell->execute(then_cmd);
    }

    return 0;
}
