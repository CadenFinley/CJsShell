#include "source_command.h"
#include "shell.h"
#include "shell_script_interpreter.h"
#include <iostream>

// Forward declaration
extern std::unique_ptr<Shell> g_shell;

int source_command(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "No source file specified" << std::endl;
        return 1;
    }
    
    if (g_shell && g_shell->get_shell_script_interpreter()) {
        return g_shell->get_shell_script_interpreter()->execute_script(args[1]) ? 0 : 1;
    } else {
        std::cerr << "Script interpreter not available" << std::endl;
        return 1;
    }
}
