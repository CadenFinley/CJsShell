#include "builtins_completions_handler.h"

#include <unordered_map>
#include <utility>

namespace builtin_completions {
namespace {

CommandDoc make_doc(std::string summary, std::vector<CompletionEntry> entries) {
    CommandDoc doc;
    doc.summary = std::move(summary);
    doc.summary_present = !doc.summary.empty();
    doc.entries = std::move(entries);
    return doc;
}

CompletionEntry make_option(std::string text, std::string description) {
    return CompletionEntry{std::move(text), std::move(description), EntryKind::Option};
}

CompletionEntry make_subcommand(std::string text, std::string description) {
    return CompletionEntry{std::move(text), std::move(description), EntryKind::Subcommand};
}

const std::unordered_map<std::string, CommandDoc>& builtin_command_docs() {
    static const std::unordered_map<std::string, CommandDoc> docs = []() {
        std::unordered_map<std::string, CommandDoc> map;

        auto add_doc = [&](std::string key, std::string summary,
                           std::vector<CompletionEntry> entries) {
            map.emplace(std::move(key), make_doc(std::move(summary), std::move(entries)));
        };

        auto add_alias = [&](const std::string& alias, const std::string& target) {
            auto it = map.find(target);
            if (it != map.end()) {
                map.emplace(alias, it->second);
            }
        };

        add_doc("abbr", "Manage interactive abbreviations", {});
        add_doc("unabbr", "Remove interactive abbreviations", {});
        add_alias("abbreviate", "abbr");
        add_alias("unabbreviate", "unabbr");

        add_doc("alias", "Create or inspect command aliases", {});
        add_doc("unalias", "Remove command aliases", {});

        add_doc("break", "Exit the innermost enclosing loop", {});
        add_doc("continue", "Advance to the next loop iteration", {});
        add_doc("return", "Exit the current function with an optional status", {});

        add_doc("cd", "Change the current directory", {});
        add_doc("pwd", "Print the current working directory",
                {make_option("-L", "Use logical path from PWD"),
                 make_option("--logical", "Use logical path from PWD"),
                 make_option("-P", "Resolve the physical path"),
                 make_option("--physical", "Resolve the physical path"),
                 make_option("--version", "Show version information")});

        add_doc("echo", "Write arguments to standard output",
                {make_option("-n", "Suppress trailing newline"),
                 make_option("-e", "Enable backslash escapes"),
                 make_option("-E", "Disable backslash escapes")});
        add_doc("printf", "Format and print data", {});

        add_doc("true", "Exit with a zero status", {});
        add_doc("false", "Exit with a non-zero status", {});
        add_doc(":", "No-op that always succeeds", {});

        add_doc("local", "Declare variables local to the current function", {});
        add_doc("export", "Export environment variables", {});
        add_doc("unset", "Remove variables from the environment", {});
        add_doc("set", "Configure shell options or positional parameters",
                {make_option("-e", "Exit immediately on errors"),
                 make_option("+e", "Disable exit-on-error"),
                 make_option("-C", "Enable noclobber"),
                 make_option("+C", "Disable noclobber"),
                 make_option("-u", "Treat unset variables as errors"),
                 make_option("+u", "Allow unset variables"),
                 make_option("-x", "Print commands before execution"),
                 make_option("+x", "Stop printing commands"),
                 make_option("-v", "Print shell input lines"),
                 make_option("+v", "Stop printing input lines"),
                 make_option("-n", "Read commands without executing"),
                 make_option("+n", "Resume executing commands"),
                 make_option("-f", "Disable pathname expansion"),
                 make_option("+f", "Enable pathname expansion"),
                 make_option("-a", "Auto-export modified variables"),
                 make_option("+a", "Stop auto-exporting variables"),
                 make_option("-o", "Set option by name"),
                 make_option("+o", "Unset option by name"),
                 make_option("--errexit-severity=", "Set errexit sensitivity level"),
                 make_option("--", "Treat remaining arguments as positional parameters")});

        add_doc("shift", "Rotate positional parameters", {});

        add_doc("source", "Execute commands from a file in the current shell", {});
        add_alias(".", "source");

        add_doc("help", "Display the builtin command reference", {});
        add_doc("version", "Show cjsh version information", {});
        add_doc("eval", "Evaluate arguments as shell code", {});
        add_doc("if", "Evaluate a conditional block", {});
        add_doc("login-startup-arg", "Add cjsh startup flags",
                {make_option("--login", "Run cjsh as a login shell"),
                 make_option("--interactive", "Force interactive mode"),
                 make_option("--debug", "Enable verbose startup diagnostics"),
                 make_option("--no-prompt", "Use a minimal prompt"),
                 make_option("--no-themes", "Disable prompt theming"),
                 make_option("--no-colors", "Disable color output"),
                 make_option("--no-titleline", "Disable terminal title updates"),
                 make_option("--show-startup-time", "Display startup timing"),
                 make_option("--no-source", "Skip sourcing configuration files"),
                 make_option("--no-completions", "Disable completion initialization"),
                 make_option("--no-syntax-highlighting", "Disable syntax highlighting"),
                 make_option("--no-smart-cd", "Disable smart cd behavior"),
                 make_option("--minimal", "Disable cjsh enhancements"),
                 make_option("--startup-test", "Enable startup test mode")});

        add_doc("syntax", "Check scripts or command strings for issues",
                {make_option("-q", "Only report the error count"),
                 make_option("--quiet", "Only report the error count"),
                 make_option("-v", "Show detailed errors"),
                 make_option("--verbose", "Show detailed errors"),
                 make_option("--no-suggestions", "Suppress fix suggestions"),
                 make_option("--no-context", "Hide offending line context"),
                 make_option("--comprehensive", "Run all validation checks"),
                 make_option("--semantic", "Include semantic validation"),
                 make_option("--style", "Include style checks"),
                 make_option("--performance", "Include performance analysis"),
                 make_option("--severity", "Filter by severity level"),
                 make_option("--category", "Filter by category"),
                 make_option("-c", "Validate the remaining arguments as a command string")});

        add_doc("history", "Show command history", {});
        add_doc("fc", "Edit or list commands from history",
                {make_option("-e", "Select editor for editing"),
                 make_option("-l", "List matching commands"),
                 make_option("-n", "Suppress line numbers when listing"),
                 make_option("-r", "Reverse the order when listing"),
                 make_option("-s", "Re-execute with substitution"),
                 make_option("-c", "Edit the provided string")});

        add_doc("exit", "Exit the shell with an optional status", {});
        add_alias("quit", "exit");

        add_doc("test", "Evaluate conditional expressions", {});
        add_alias("[", "test");
        add_doc("[[", "Evaluate extended conditional expressions", {});

        add_doc("exec", "Replace the shell with another program", {});

        add_doc("command", "Run a command bypassing functions",
                {make_option("-p", "Use a default PATH"),
                 make_option("-v", "Print a short description"),
                 make_option("-V", "Print a verbose description"),
                 make_option("--", "Stop processing options")});

        add_doc(
            "trap", "Set or list signal handlers",
            {make_option("-l", "List available signals"), make_option("-p", "Show current traps")});

        add_doc(
            "jobs", "List background jobs",
            {make_option("-l", "Show PIDs and status"), make_option("-p", "Print job PIDs only")});
        add_doc("fg", "Bring a job to the foreground", {});
        add_doc("bg", "Resume a job in the background", {});
        add_doc("wait", "Wait for jobs or processes to finish", {});

        add_doc(
            "kill", "Send signals to processes or jobs",
            {make_option("-l", "List signal names"), make_option("-s", "Specify signal by name"),
             make_option("-HUP", "Send the HUP signal"), make_option("-INT", "Send the INT signal"),
             make_option("-TERM", "Send the TERM signal"),
             make_option("-KILL", "Send the KILL signal"),
             make_option("-STOP", "Send the STOP signal"),
             make_option("-USR1", "Send the USR1 signal"),
             make_option("-USR2", "Send the USR2 signal")});

        add_doc("readonly", "Mark variables as read-only",
                {make_option("-p", "Print current readonly variables"),
                 make_option("-f", "Operate on functions (not yet implemented)")});

        add_doc("read", "Read a line from standard input",
                {make_option("-r", "Disable backslash escapes"),
                 make_option("-n", "Read a specific number of characters"),
                 make_option("-p", "Display a prompt"), make_option("-d", "Use a custom delimiter"),
                 make_option("-t", "Set a timeout (not yet implemented)")});

        add_doc("umask", "Set or display the file mode creation mask",
                {make_option("-p", "Print in reusable format"),
                 make_option("-S", "Display the mask symbolically")});

        add_doc(
            "ulimit", "Display or set resource limits",
            {make_option("-a", "Show all current limits"), make_option("-H", "Use hard limits"),
             make_option("-S", "Use soft limits"), make_option("-c", "Limit core file size"),
             make_option("-d", "Limit data segment size"), make_option("-f", "Limit file size"),
             make_option("-l", "Limit locked-in-memory size"),
             make_option("-m", "Limit resident set size"),
             make_option("-n", "Limit open file descriptors"),
             make_option("-q", "Limit POSIX message queue bytes"),
             make_option("-r", "Limit realtime priority"), make_option("-s", "Limit stack size"),
             make_option("-t", "Limit CPU time"), make_option("-u", "Limit user processes"),
             make_option("-v", "Limit virtual memory"), make_option("-w", "Limit swap size"),
             make_option("--all", "Show all current limits"),
             make_option("--hard", "Use hard limits"), make_option("--soft", "Use soft limits")});

        add_doc("getopts", "Parse positional parameters as options", {});
        add_doc("times", "Display accumulated process times", {});

        add_doc(
            "type", "Describe how commands are resolved",
            {make_option("-a", "Show all possible resolutions"),
             make_option("-f", "Force ignoring shell functions"),
             make_option("-p", "Force PATH lookup"), make_option("-t", "Print the type keyword"),
             make_option("-P", "Search the default PATH"),
             make_option("--", "Stop processing options")});

        add_doc("which", "Locate commands in PATH",
                {make_option("-a", "Show all matches"), make_option("-s", "Silent mode"),
                 make_option("--", "Stop processing options")});

        add_doc("validate", "Toggle command validation or check names",
                {make_subcommand("on", "Enable command validation"),
                 make_subcommand("off", "Disable command validation"),
                 make_subcommand("status", "Show whether validation is enabled")});

        add_doc("hash", "Manage the command lookup cache",
                {make_option("-r", "Reset cached entries"),
                 make_option("-d", "Disable caching for specified names")});

        add_doc("generate-completions", "Regenerate cached external completions",
                {make_option("--quiet", "Suppress per-command output"),
                 make_option("-q", "Suppress per-command output"),
                 make_option("--force", "Force regeneration even if cached"),
                 make_option("-f", "Force regeneration even if cached"),
                 make_option("--no-force", "Reuse existing cache entries"),
                 make_option("-j", "Set the number of parallel jobs"),
                 make_option("--jobs", "Set the number of parallel jobs"),
                 make_option("--", "Treat remaining arguments as command names")});

        add_doc("hook", "Manage shell lifecycle hooks",
                {make_subcommand("add", "Register a function for a hook"),
                 make_subcommand("remove", "Unregister a function"),
                 make_subcommand("list", "Show registered hooks"),
                 make_subcommand("clear", "Remove hooks for a type")});

        add_doc("hook-add", "",
                {make_subcommand("precmd", "Run before the prompt"),
                 make_subcommand("preexec", "Run before executing commands"),
                 make_subcommand("chpwd", "Run after changing directories")});
        add_doc("hook-remove", "",
                {make_subcommand("precmd", "Run before the prompt"),
                 make_subcommand("preexec", "Run before executing commands"),
                 make_subcommand("chpwd", "Run after changing directories")});
        add_doc("hook-clear", "",
                {make_subcommand("precmd", "Run before the prompt"),
                 make_subcommand("preexec", "Run before executing commands"),
                 make_subcommand("chpwd", "Run after changing directories")});
        add_doc("hook-list", "",
                {make_subcommand("precmd", "Run before the prompt"),
                 make_subcommand("preexec", "Run before executing commands"),
                 make_subcommand("chpwd", "Run after changing directories")});

        add_doc("builtin", "Invoke a builtin bypassing functions", {});

        add_doc("cjsh-widget", "Invoke an interactive widget",
                {make_subcommand("get-buffer", "Print the current input buffer"),
                 make_subcommand("set-buffer", "Replace the input buffer content"),
                 make_subcommand("get-cursor", "Show the cursor position"),
                 make_subcommand("set-cursor", "Move the cursor to a byte offset"),
                 make_subcommand("insert", "Insert text at the cursor"),
                 make_subcommand("append", "Append text to the buffer"),
                 make_subcommand("clear", "Clear the input buffer"),
                 make_subcommand("accept", "Accept and submit the current buffer")});

        add_doc("cjshopt", "Configure cjsh interactive behavior",
                {make_subcommand("style_def", "Define syntax highlight styles"),
                 make_subcommand("login-startup-arg", "Add a startup flag"),
                 make_subcommand("completion-case", "Configure completion case sensitivity"),
                 make_subcommand("completion-spell", "Configure completion spell correction"),
                 make_subcommand("line-numbers", "Configure multiline line numbers"),
                 make_subcommand("current-line-number-highlight",
                                 "Toggle current line number highlighting"),
                 make_subcommand("multiline-start-lines", "Set default multiline prompt height"),
                 make_subcommand("hint-delay", "Adjust inline hint delay"),
                 make_subcommand("completion-preview", "Toggle completion preview"),
                 make_subcommand("visible-whitespace", "Toggle visible whitespace"),
                 make_subcommand("hint", "Toggle inline hints"),
                 make_subcommand("multiline-indent", "Toggle multiline auto-indent"),
                 make_subcommand("multiline", "Toggle multiline input"),
                 make_subcommand("inline-help", "Toggle inline help"),
                 make_subcommand("auto-tab", "Toggle automatic tab completion"),
                 make_subcommand("keybind", "Inspect or modify key bindings"),
                 make_subcommand("generate-profile", "Generate ~/.cjprofile"),
                 make_subcommand("generate-rc", "Generate ~/.cjshrc"),
                 make_subcommand("generate-logout", "Generate ~/.cjsh_logout"),
                 make_subcommand("set-max-bookmarks", "Limit stored directory bookmarks"),
                 make_subcommand("set-history-max", "Configure history persistence"),
                 make_subcommand("bookmark-blacklist", "Manage bookmark exclusions")});

        add_doc("cjshopt-style_def", "Define or reset syntax styles",
                {make_option("--reset", "Reset all highlight styles to defaults")});

        add_doc("cjshopt-login-startup-arg", "Add cjsh startup flags",
                {make_option("--login", "Run cjsh as a login shell"),
                 make_option("--interactive", "Force interactive mode"),
                 make_option("--debug", "Enable verbose startup diagnostics"),
                 make_option("--no-prompt", "Use a minimal prompt"),
                 make_option("--no-themes", "Disable prompt theming"),
                 make_option("--no-colors", "Disable color output"),
                 make_option("--no-titleline", "Disable terminal title updates"),
                 make_option("--show-startup-time", "Display startup timing"),
                 make_option("--no-source", "Skip sourcing configuration files"),
                 make_option("--no-completions", "Disable completion initialization"),
                 make_option("--no-syntax-highlighting", "Disable syntax highlighting"),
                 make_option("--no-smart-cd", "Disable smart cd behavior"),
                 make_option("--minimal", "Disable cjsh enhancements"),
                 make_option("--startup-test", "Enable startup test mode")});

        add_doc("cjshopt-hint-delay", "Adjust inline hint delay",
                {make_subcommand("status", "Show the current delay in milliseconds"),
                 make_option("--status", "Show the current delay in milliseconds")});

        add_doc("cjshopt-set-max-bookmarks", "Limit stored directory bookmarks", {});

        add_doc("cjshopt-set-history-max", "Configure history persistence",
                {make_subcommand("default", "Restore the default history limit"),
                 make_option("--default", "Restore the default history limit"),
                 make_subcommand("status", "Display the current history limit"),
                 make_option("--status", "Display the current history limit")});

        add_doc("cjshopt-keybind", "Inspect or modify key bindings",
                {make_subcommand("list", "Show current key bindings"),
                 make_subcommand("set", "Replace bindings for an action"),
                 make_subcommand("add", "Add bindings for an action"),
                 make_subcommand("clear", "Remove bindings for key sequences"),
                 make_subcommand("clear-action", "Remove bindings for an action"),
                 make_subcommand("reset", "Restore default key bindings"),
                 make_subcommand("profile", "Manage key binding profiles"),
                 make_subcommand("ext", "Manage command key bindings")});

        add_doc("cjshopt-keybind-profile", "Manage key binding profiles",
                {make_subcommand("list", "List available key binding profiles"),
                 make_subcommand("set", "Activate a key binding profile")});

        add_doc("cjshopt-keybind-ext", "Manage custom command key bindings",
                {make_subcommand("list", "Show custom command key bindings"),
                 make_subcommand("set", "Bind a key to a shell command"),
                 make_subcommand("clear", "Remove custom command key bindings"),
                 make_subcommand("reset", "Clear all custom command key bindings")});

        add_doc("cjshopt-generate-profile", "Generate ~/.cjprofile",
                {make_option("--force", "Overwrite the existing profile"),
                 make_option("-f", "Overwrite the existing profile"),
                 make_option("--alt", "Write to the alternate configuration path"),
                 make_option("--help", "Show usage information"),
                 make_option("-h", "Show usage information")});

        add_doc("cjshopt-generate-rc", "Generate ~/.cjshrc",
                {make_option("--force", "Overwrite the existing rc file"),
                 make_option("-f", "Overwrite the existing rc file"),
                 make_option("--alt", "Write to the alternate configuration path"),
                 make_option("--help", "Show usage information"),
                 make_option("-h", "Show usage information")});

        add_doc("cjshopt-generate-logout", "Generate ~/.cjsh_logout",
                {make_option("--force", "Overwrite the existing logout file"),
                 make_option("-f", "Overwrite the existing logout file"),
                 make_option("--alt", "Write to the alternate configuration path"),
                 make_option("--help", "Show usage information"),
                 make_option("-h", "Show usage information")});

        add_doc("cjshopt-completion-case", "",
                {make_subcommand("on", "Enable case-sensitive matches"),
                 make_subcommand("off", "Disable case sensitivity"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-completion-spell", "",
                {make_subcommand("on", "Enable spell correction"),
                 make_subcommand("off", "Disable spell correction"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-completion-preview", "",
                {make_subcommand("on", "Enable completion preview"),
                 make_subcommand("off", "Disable completion preview"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-visible-whitespace", "",
                {make_subcommand("on", "Show whitespace markers"),
                 make_subcommand("off", "Hide whitespace markers"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-hint", "",
                {make_subcommand("on", "Enable inline hints"),
                 make_subcommand("off", "Disable inline hints"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-multiline-indent", "",
                {make_subcommand("on", "Enable multiline auto-indent"),
                 make_subcommand("off", "Disable multiline auto-indent"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-multiline", "",
                {make_subcommand("on", "Enable multiline input"),
                 make_subcommand("off", "Disable multiline input"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-inline-help", "",
                {make_subcommand("on", "Enable inline help"),
                 make_subcommand("off", "Disable inline help"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-auto-tab", "",
                {make_subcommand("on", "Enable automatic tab completion"),
                 make_subcommand("off", "Disable automatic tab completion"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-line-numbers", "",
                {make_subcommand("on", "Enable absolute line numbers"),
                 make_subcommand("off", "Hide line numbers"),
                 make_subcommand("relative", "Show relative line numbers"),
                 make_subcommand("absolute", "Show absolute line numbers"),
                 make_subcommand("status", "Show current setting")});
        add_doc("cjshopt-multiline-start-lines", "",
                {make_subcommand("status", "Show current multiline height")});
        add_doc("cjshopt-bookmark-blacklist", "",
                {make_subcommand("add", "Add a directory to the blacklist"),
                 make_subcommand("remove", "Remove a directory from the blacklist"),
                 make_subcommand("list", "List blacklisted directories"),
                 make_subcommand("clear", "Clear the blacklist")});

        return map;
    }();
    return docs;
}

}  // namespace

const CommandDoc* lookup_builtin_command_doc(const std::string& doc_target) {
    const auto& docs = builtin_command_docs();
    auto it = docs.find(doc_target);
    if (it != docs.end())
        return &it->second;
    return nullptr;
}

std::string get_builtin_summary(const std::string& command) {
    if (const auto* doc = lookup_builtin_command_doc(command))
        return doc->summary;
    return {};
}

}  // namespace builtin_completions
