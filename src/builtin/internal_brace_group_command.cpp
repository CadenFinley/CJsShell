#include "internal_brace_group_command.h"

#include "shell.h"

int internal_brace_group_command(const std::vector<std::string>& args, Shell* shell) {
    if (shell == nullptr) {
        return 1;
    }

    if (args.size() < 2) {
        return 0;
    }

    const std::string& group_content = args[1];
    if (group_content.empty()) {
        return 0;
    }

    return shell->execute(group_content);
}
