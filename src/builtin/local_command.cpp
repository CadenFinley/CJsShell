#include "local_command.h"

#include <iostream>
#include "cjsh.h"
#include "error_out.h"
#include "shell.h"
#include "shell_script_interpreter.h"

int local_command(const std::vector<std::string>& args, Shell* shell) {
    if (args.size() == 1) {
        return 0;
    }

    auto script_interpreter = shell->get_shell_script_interpreter();
    if (!script_interpreter) {
        print_error({ErrorType::RUNTIME_ERROR, "local", "not available outside of functions", {}});
        return 1;
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
            std::string name = arg;

            if (name.empty()) {
                print_error({ErrorType::INVALID_ARGUMENT, "local", "invalid variable name", {}});
                all_successful = false;
                continue;
            }

            const char* current_value = getenv(name.c_str());
            std::string value = current_value ? current_value : "";

            script_interpreter->set_local_variable(name, value);
        }
    }

    return all_successful ? 0 : 1;
}