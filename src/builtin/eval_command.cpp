#include "eval_command.h"

#include "builtin_help.h"

#include "cjsh.h"
#include "error_out.h"
#include "shell.h"

int eval_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: eval STRING", "Evaluate STRING in the current shell context."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "eval", "missing arguments", {}});
        return 1;
    }

    std::string command_to_eval;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) {
            command_to_eval += " ";
        }
        command_to_eval += args[i];
    }
    if (shell) {
        int result = shell->execute(command_to_eval);
        return result;
    } else {
        print_error({ErrorType::RUNTIME_ERROR, "eval", "shell not initialized", {}});
        return 1;
    }
}
