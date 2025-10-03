#include "source_command.h"

#include "builtin_help.h"

#include <filesystem>

#include "error_out.h"
#include "shell.h"

extern std::unique_ptr<Shell> g_shell;

int source_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: source FILE",
                             "Execute commands from FILE in the current shell environment."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "source", "missing file operand", {}});
        return 1;
    }

    if (!g_shell) {
        print_error({ErrorType::RUNTIME_ERROR, "source", "shell not initialized", {}});
        return 1;
    }

    return g_shell->execute_script_file(std::filesystem::path(args[1]));
}
