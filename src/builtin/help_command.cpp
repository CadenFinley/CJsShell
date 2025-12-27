#include "help_command.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "usage.h"
#include "version_command.h"

int help_command() {
    const std::string separator(80, '-');

    auto heading = [&](const std::string& title) {
        std::cout << "\n" << title << "\n" << separator << "\n";
    };

    std::cout << "\nCJSH QUICK REFERENCE\n" << separator << "\n";
    (void)version_command({});

    heading("Project source");
    std::cout << "  Git repository:  https://github.com/CadenFinley/CJsShell\n";
    std::cout << "  Documentation:   https://cadenfinley.github.io/CJsShell/\n";

    heading("Built-in commands");
    struct BuiltinInfo {
        const char* name;
        const char* description;
    };

    const std::vector<BuiltinInfo> builtins = {
        // Navigation and file system
        {"cd", "Change the current directory (smart cd by default)"},
        {"pwd", "Print the current working directory"},

        // Output and formatting
        {"echo", "Print arguments separated by spaces"},
        {"printf", "Format and print data using printf-style specifiers"},

        // Shell control
        {"help", "Display this overview"},
        {"version", "Show cjsh version and build information"},
        {"exit / quit", "Leave the shell with an optional exit status"},

        // Script execution
        {"eval", "Evaluate a string as shell code"},
        {"exec", "Replace the shell process with another program"},
        {"source / .", "Execute commands from a file in the current shell"},
        {"command", "Execute command bypassing functions and aliases"},
        {"builtin", "Run a builtin directly, bypassing functions and PATH"},

        // Variables and environment
        {"set", "Adjust shell options or positional parameters"},
        {"shift", "Rotate positional parameters to the left"},
        {"export", "Set or display environment variables"},
        {"unset", "Remove environment variables"},
        {"local", "Declare local variables inside functions"},
        {"readonly", "Mark variables as read-only"},

        // Input/output
        {"read", "Read user input into variables"},
        {"getopts", "Parse positional parameters as short options"},

        // Aliases and abbreviations
        {"alias", "Create or list command aliases"},
        {"unalias", "Remove command aliases"},
        {"abbr", "Create or list command abbreviations"},
        {"unabbr", "Remove command abbreviations"},

        // Command lookup and caching
        {"type", "Explain how a command name will be resolved"},
        {"which", "Locate executables in PATH"},
        {"hash", "Cache command lookups or display the cache"},

        // History
        {"history", "Display command history"},
        {"fc", "Fix command - edit and re-execute commands from history"},

        // Job control
        {"jobs", "List background jobs"},
        {"fg", "Bring a job to the foreground"},
        {"bg", "Resume a job in the background"},
        {"wait", "Wait for jobs or processes to finish"},
        {"kill", "Send signals to jobs or processes"},

        // System
        {"umask", "Show or set the file creation mask"},
        {"ulimit", "Set or show resource limits"},
        {"trap", "Set signal handlers or list existing traps"},
        {"times", "Show CPU usage for the shell and its children"},

        // Flow control
        {"break", "Exit the current loop"},
        {"continue", "Skip to the next loop iteration"},
        {"return", "Exit the current function with an optional status"},

        // Testing and conditionals
        {"test / [", "Evaluate POSIX test expressions"},
        {"[[", "Evaluate extended test expressions"},
        {"if", "Run conditional blocks in scripts"},
        {":", "No-op command that always succeeds"},
        {"true", "Return success (exit code 0)"},
        {"false", "Return failure (exit code 1)"},

        // Shell customization
        {"cjshopt", "Generate config files and adjust cjsh options"},
        {"hook", "Manage shell hooks (precmd, preexec, chpwd)"}};

    std::cout << std::left;
    constexpr int column_width = 20;
    for (const auto& item : builtins) {
        std::cout << "  " << std::setw(column_width) << item.name << item.description << "\n";
    }
    std::cout << "\n  Note: Use '<command> --help' to see detailed usage for most commands.\n";

    heading("Shell scripting features");
    std::cout << "  - POSIX-style functions with local variables and return codes.\n";
    std::cout << "  - Conditionals with if/elif/else/fi plus test, [, and [[ expressions.\n";
    std::cout << "  - Loop constructs (for/while/until) and loop controls (break/continue).\n";
    std::cout << "  - Command substitution $(...), pipelines, redirection, and here-strings.\n";
    std::cout << "  - Script tooling: source plus built-in inspection utilities like 'type',\n"
                 "    'which', and 'hash' for verifying commands before execution.\n";

    heading("Startup and shutdown");
    std::cout << "  Startup sequence:\n";
    std::cout << "    1. Login shells load ~/.profile (if present) then ~/.cjprofile.\n";
    std::cout << "    2. Stored startup flags from 'cjshopt login-startup-arg' are applied.\n";
    std::cout << "    3. Interactive mode initializes colors, completions, and sources ~/.cjshrc\n"
                 "       unless disabled with --no-source or secure mode.\n";
    std::cout << "  Shutdown sequence:\n";
    std::cout << "    - Registered EXIT traps run before teardown.\n";
    std::cout << "    - ~/.cjsh_logout is sourced for interactive sessions (when it exists).\n";
    std::cout << "    - History and themes are flushed before exit.\n";

    heading("Primary cjsh directories");
    std::cout << "  ~/.cjprofile        Login configuration and persisted startup flags.\n";
    std::cout << "  ~/.cjshrc           Interactive configuration (aliases, themes).\n";
    std::cout << "  ~/.cjsh_logout      Optional logout script sourced on exit.\n";
    std::cout << "  ~/.cache/cjsh/      Cache directory (history.txt, exec cache).\n";
    std::cout << "  ~/.cache/cjsh/.first_boot  Marker used to suppress the first-run banner.\n";

    heading("cjsh invocation and startup flags");
    print_usage(false, false, false);

    heading("Isocline line editing");
    std::cout << "  - cjsh embeds the isocline line editor for multiline input, highlighting,\n"
                 "    and completion popups.\n";
    std::cout << "  - Press <Tab> for context-aware completions and suggestions.\n";
    std::cout << "  - Press F1 to open isocline's interactive cheat sheet of key bindings.\n";
    std::cout << "  - Incremental history search (Ctrl+R) and other readline-style shortcuts are "
                 "available.\n";
    std::cout
        << "  - Configuration such as syntax colors can be adjusted via 'cjshopt style_def'.\n";

    std::cout << "\n" << separator << "\n";
    return 0;
}
