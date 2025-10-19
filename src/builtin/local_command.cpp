#include "local_command.h"

#include "builtin_help.h"

#include "cjsh.h"
#include "error_out.h"
#include "shell.h"
#include "shell_script_interpreter.h"

int local_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: local NAME[=VALUE] ...",
                                   "Define local variables within a function scope."})) {
        return 0;
    }

    auto* script_interpreter = shell->get_shell_script_interpreter();
    if (script_interpreter == nullptr || !script_interpreter->in_function_scope()) {
        print_error({ErrorType::RUNTIME_ERROR, "local", "not available outside of functions", {}});
        return 1;
    }

    if (args.size() == 1) {
        return 0;
    }

    bool all_successful = true;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = arg.substr(0, eq_pos);
            std::string value = arg.substr(eq_pos + 1);

            if (name.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT, "local", "invalid variable name", {}});
                all_successful = false;
                continue;
            }

            script_interpreter->set_local_variable(name, value);
        } else {
            const std::string& name = arg;

            if (name.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT, "local", "invalid variable name", {}});
                all_successful = false;
                continue;
            }

            
            script_interpreter->set_local_variable(name, "");
        }
    }

    return all_successful ? 0 : 1;
}
