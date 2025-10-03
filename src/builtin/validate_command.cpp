#include "validate_command.h"

#include "builtin_help.h"

#include <iostream>

#include "cjsh.h"
#include "error_out.h"
#include "parser.h"
#include "shell.h"
#include "suggestion_utils.h"

int validate_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args,
                            {"Usage: validate on|off|status|COMMAND ...",
                             "Toggle command validation or check specific command names."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "validate",
                     "usage: validate [on|off|status] or validate <command_name>...",
                     {}});
        return 1;
    }

    auto* parser = shell ? shell->get_parser() : nullptr;
    if (!parser) {
        print_error({ErrorType::RUNTIME_ERROR, "validate", "Parser not available", {}});
        return 1;
    }

    if (args[1] == "on") {
        parser->set_command_validation_enabled(true);
        std::cout << "Command validation enabled" << std::endl;
        return 0;
    } else if (args[1] == "off") {
        parser->set_command_validation_enabled(false);
        std::cout << "Command validation disabled" << std::endl;
        return 0;
    } else if (args[1] == "status") {
        std::cout << "Command validation is "
                  << (parser->get_command_validation_enabled() ? "enabled" : "disabled")
                  << std::endl;
        return 0;
    }

    int exit_code = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& cmd_name = args[i];

        if (parser->is_valid_command(cmd_name)) {
            std::cout << cmd_name << ": valid command" << std::endl;
        } else {
            auto suggestions = suggestion_utils::generate_command_suggestions(cmd_name);
            ErrorInfo error = {ErrorType::COMMAND_NOT_FOUND, cmd_name, "command not found",
                               suggestions};
            print_error(error);

            exit_code = 1;
        }
    }

    return exit_code;
}