#include "builtin_argument_completion.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

#include "builtin/builtin.h"
#include "cjsh.h"
#include "completion_tracker.h"
#include "completion_utils.h"
#include "job_control.h"
#include "shell.h"
#include "shell_script_interpreter.h"

namespace builtin_argument_completion {
namespace {

struct Suggestion {
    std::string text;
    bool append_space;
};

constexpr const char* kCompletionSource = "builtin argument";

const std::string& current_token(const std::vector<std::string>& args) {
    static const std::string kEmpty;
    if (args.empty())
        return kEmpty;
    return args.back();
}

size_t completed_arg_count(const std::vector<std::string>& args) {
    if (args.empty())
        return 0;
    return args.size() - 1;
}

bool equals_token(const std::string& value, const char* token) {
    return completion_utils::equals_completion_token(value, token);
}

void add_tokens(std::vector<Suggestion>& out, std::initializer_list<const char*> tokens,
                bool append_space = true) {
    for (const char* token : tokens) {
        out.push_back({token, append_space});
    }
}

void add_tokens(std::vector<Suggestion>& out, const std::vector<std::string>& tokens,
                bool append_space = true) {
    for (const auto& token : tokens) {
        out.push_back({token, append_space});
    }
}

bool is_first_argument(const std::vector<std::string>& args) {
    return completed_arg_count(args) == 0;
}

const std::string& previous_token(const std::vector<std::string>& args) {
    static const std::string kEmpty;
    if (args.size() < 2)
        return kEmpty;
    return args[args.size() - 2];
}

void add_help_flags_if_first(std::vector<Suggestion>& suggestions,
                             const std::vector<std::string>& args) {
    if (is_first_argument(args))
        add_tokens(suggestions, {"--help", "-h"});
}

std::vector<std::string> sorted_unique(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

std::vector<std::string> gather_alias_names() {
    std::vector<std::string> names;
    if (!g_shell)
        return names;
    const auto& alias_map = g_shell->get_aliases();
    names.reserve(alias_map.size());
    for (const auto& [name, _] : alias_map) {
        names.push_back(name);
    }
    return sorted_unique(std::move(names));
}

std::vector<std::string> gather_abbreviation_names() {
    std::vector<std::string> names;
    if (!g_shell)
        return names;
    const auto& abbr_map = g_shell->get_abbreviations();
    names.reserve(abbr_map.size());
    for (const auto& [name, _] : abbr_map) {
        names.push_back(name);
    }
    return sorted_unique(std::move(names));
}

std::vector<std::string> gather_builtin_names() {
    std::vector<std::string> names;
    if (!g_shell || g_shell->get_built_ins() == nullptr)
        return names;

    auto raw = g_shell->get_built_ins()->get_builtin_commands();
    names.reserve(raw.size());
    for (const auto& name : raw) {
        if (name.rfind("__", 0) == 0)
            continue;
        names.push_back(name);
    }
    return sorted_unique(std::move(names));
}

std::vector<std::string> gather_job_specifiers() {
    std::vector<std::string> specs;
    auto jobs = JobManager::instance().get_all_jobs();
    specs.reserve(jobs.size());
    for (const auto& job : jobs) {
        if (!job)
            continue;
        specs.push_back("%" + std::to_string(job->job_id));
    }
    return sorted_unique(std::move(specs));
}

std::vector<std::string> gather_hook_function_names() {
    std::vector<std::string> functions;
    if (!g_shell)
        return functions;

    auto* interpreter = g_shell->get_shell_script_interpreter();
    if (interpreter == nullptr)
        return functions;

    functions = interpreter->get_function_names();
    std::sort(functions.begin(), functions.end());
    functions.erase(std::unique(functions.begin(), functions.end()), functions.end());
    return functions;
}

std::vector<Suggestion> complete_validate(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"on", "off", "status"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_command_builtin(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-p", "-v", "-V"}, false);
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_cjsh_widget(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (completed_arg_count(args) == 0) {
        add_tokens(suggestions, {"get-buffer", "set-buffer", "get-cursor", "set-cursor", "insert",
                                 "append", "clear", "accept"});
    }
    return suggestions;
}

std::vector<Suggestion> complete_hook(const std::vector<std::string>& args) {
    static const std::vector<std::string> kHookTypes = {"precmd", "preexec", "chpwd"};

    std::vector<Suggestion> suggestions;
    size_t completed = completed_arg_count(args);

    if (completed == 0) {
        add_tokens(suggestions, {"add", "remove", "list", "clear"});
        add_help_flags_if_first(suggestions, args);
        return suggestions;
    }

    const std::string& subcommand = args.front();

    if (equals_token(subcommand, "add") || equals_token(subcommand, "remove")) {
        if (completed == 1) {
            add_tokens(suggestions, kHookTypes);
        } else if (completed == 2) {
            auto functions = gather_hook_function_names();
            add_tokens(suggestions, functions);
        }
    } else if (equals_token(subcommand, "clear") || equals_token(subcommand, "list")) {
        if (completed == 1)
            add_tokens(suggestions, kHookTypes);
    }

    return suggestions;
}

std::vector<Suggestion> complete_cjshopt(const std::vector<std::string>& args) {
    static const std::vector<std::string> kTopLevelSubcommands = {"style_def",
                                                                  "login-startup-arg",
                                                                  "completion-case",
                                                                  "completion-spell",
                                                                  "line-numbers",
                                                                  "current-line-number-highlight",
                                                                  "multiline-start-lines",
                                                                  "hint-delay",
                                                                  "completion-preview",
                                                                  "visible-whitespace",
                                                                  "hint",
                                                                  "multiline-indent",
                                                                  "multiline",
                                                                  "inline-help",
                                                                  "auto-tab",
                                                                  "keybind",
                                                                  "generate-profile",
                                                                  "generate-rc",
                                                                  "generate-logout",
                                                                  "set-max-bookmarks",
                                                                  "set-history-max",
                                                                  "bookmark-blacklist"};

    static const std::vector<std::string> kToggleTokens = {"on",       "off",    "status",
                                                           "--status", "--help", "-h"};

    static const std::vector<std::string> kLoginStartupFlags = {"--login",
                                                                "--interactive",
                                                                "--debug",
                                                                "--no-prompt",
                                                                "--no-themes",
                                                                "--no-colors",
                                                                "--no-titleline",
                                                                "--show-startup-time",
                                                                "--no-source",
                                                                "--no-completions",
                                                                "--no-syntax-highlighting",
                                                                "--no-smart-cd",
                                                                "--minimal",
                                                                "--startup-test"};

    static const std::vector<std::string> kKeybindSubcommands = {
        "ext", "list", "set", "add", "clear", "clear-action", "reset", "profile", "--help", "-h"};

    static const std::vector<std::string> kKeybindProfileSubcommands = {"list", "set"};
    static const std::vector<std::string> kKeybindExtSubcommands = {"list", "set", "clear",
                                                                    "reset"};

    static const std::vector<std::string> kBookmarkBlacklistSubcommands = {
        "add", "remove", "list", "clear", "--help", "-h"};

    static const std::vector<std::string> kGenerateFlags = {"--force", "-f", "--alt", "--help",
                                                            "-h"};

    std::vector<Suggestion> suggestions;
    size_t completed = completed_arg_count(args);
    size_t current_index = args.empty() ? 0 : args.size() - 1;

    if (completed == 0) {
        add_tokens(suggestions, kTopLevelSubcommands);
        add_help_flags_if_first(suggestions, args);
        return suggestions;
    }

    if (args.empty())
        return suggestions;

    const std::string& subcommand = args.front();

    auto add_toggle_tokens = [&]() {
        if (current_index == 1)
            add_tokens(suggestions, kToggleTokens);
    };

    if (equals_token(subcommand, "completion-case") ||
        equals_token(subcommand, "completion-spell") ||
        equals_token(subcommand, "completion-preview") ||
        equals_token(subcommand, "visible-whitespace") || equals_token(subcommand, "hint") ||
        equals_token(subcommand, "multiline-indent") || equals_token(subcommand, "multiline") ||
        equals_token(subcommand, "inline-help") || equals_token(subcommand, "auto-tab") ||
        equals_token(subcommand, "current-line-number-highlight")) {
        add_toggle_tokens();
        return suggestions;
    }

    if (equals_token(subcommand, "line-numbers")) {
        if (current_index == 1)
            add_tokens(suggestions,
                       {"on", "off", "relative", "absolute", "status", "--status", "--help", "-h"});
        return suggestions;
    }

    if (equals_token(subcommand, "multiline-start-lines")) {
        if (current_index == 1)
            add_tokens(suggestions, {"status", "--status", "--help", "-h"});
        return suggestions;
    }

    if (equals_token(subcommand, "hint-delay")) {
        if (current_index == 1)
            add_tokens(suggestions, {"status", "--status", "--help", "-h"});
        return suggestions;
    }

    if (equals_token(subcommand, "set-history-max")) {
        if (current_index == 1)
            add_tokens(suggestions, {"default", "--default", "status", "--status", "--help", "-h"});
        return suggestions;
    }

    if (equals_token(subcommand, "set-max-bookmarks")) {
        if (current_index == 1)
            add_tokens(suggestions, {"--help", "-h"});
        return suggestions;
    }

    if (equals_token(subcommand, "style_def")) {
        if (current_index == 1)
            add_tokens(suggestions, {"--reset"});
        return suggestions;
    }

    if (equals_token(subcommand, "login-startup-arg")) {
        if (current_index == 1)
            add_tokens(suggestions, kLoginStartupFlags);
        return suggestions;
    }

    if (equals_token(subcommand, "keybind")) {
        if (current_index == 1) {
            add_tokens(suggestions, kKeybindSubcommands);
            return suggestions;
        }
        if (args.size() > 1 && current_index == 2) {
            const std::string& keybind_sub = args[1];
            if (equals_token(keybind_sub, "profile")) {
                add_tokens(suggestions, kKeybindProfileSubcommands);
            } else if (equals_token(keybind_sub, "ext")) {
                add_tokens(suggestions, kKeybindExtSubcommands);
            }
        }
        return suggestions;
    }

    if (equals_token(subcommand, "bookmark-blacklist")) {
        if (current_index == 1)
            add_tokens(suggestions, kBookmarkBlacklistSubcommands);
        return suggestions;
    }

    if (equals_token(subcommand, "generate-profile") || equals_token(subcommand, "generate-rc") ||
        equals_token(subcommand, "generate-logout")) {
        if (current_index == 1)
            add_tokens(suggestions, kGenerateFlags);
        return suggestions;
    }

    return suggestions;
}

std::vector<Suggestion> complete_alias(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_help_flags_if_first(suggestions, args);
        auto names = gather_alias_names();
        add_tokens(suggestions, names);
    }
    return suggestions;
}

std::vector<Suggestion> complete_unalias(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_help_flags_if_first(suggestions, args);

    auto names = gather_alias_names();
    add_tokens(suggestions, names);
    return suggestions;
}

std::vector<Suggestion> complete_abbr(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_help_flags_if_first(suggestions, args);
        auto names = gather_abbreviation_names();
        add_tokens(suggestions, names);
    }
    return suggestions;
}

std::vector<Suggestion> complete_unabbr(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_help_flags_if_first(suggestions, args);

    auto names = gather_abbreviation_names();
    add_tokens(suggestions, names);
    return suggestions;
}

std::vector<Suggestion> complete_builtin_invocation(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_help_flags_if_first(suggestions, args);
        auto names = gather_builtin_names();
        add_tokens(suggestions, names);
    }
    return suggestions;
}

std::vector<Suggestion> complete_builtin_help(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        auto names = gather_builtin_names();
        add_tokens(suggestions, names);
    }
    return suggestions;
}

std::vector<Suggestion> complete_cd(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_tokens(suggestions, {"-"});
        add_help_flags_if_first(suggestions, args);
    }
    return suggestions;
}

std::vector<Suggestion> complete_echo(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-n", "-e", "-E"}, false);
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_exit(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-f", "--force"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_fc(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_tokens(suggestions, {"-l", "-n", "-r", "-s", "-e", "-c", "--command"}, false);
        add_help_flags_if_first(suggestions, args);
    }
    return suggestions;
}

std::vector<Suggestion> complete_hash(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-r", "-d", "--"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_history(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_jobs(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-l", "-p"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_job_control_target(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    add_help_flags_if_first(suggestions, args);

    auto specs = gather_job_specifiers();
    add_tokens(suggestions, specs);
    return suggestions;
}

std::vector<Suggestion> complete_kill(const std::vector<std::string>& args) {
    static const std::vector<std::string> kSignalNames = {
        "HUP",  "INT",  "QUIT", "ILL",  "TRAP",   "ABRT", "BUS",   "FPE",  "KILL", "USR1",
        "SEGV", "USR2", "PIPE", "ALRM", "TERM",   "CHLD", "CONT",  "STOP", "TSTP", "TTIN",
        "TTOU", "URG",  "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO",   "SYS"};

    std::vector<Suggestion> suggestions;
    const std::string& token = current_token(args);
    size_t completed = completed_arg_count(args);

    if (completed == 0 && (token.empty() || token[0] == '-'))
        add_tokens(suggestions, {"-l"});

    if (completed == 0) {
        for (const auto& sig : kSignalNames) {
            suggestions.push_back({"-" + sig, true});
        }
    }

    add_help_flags_if_first(suggestions, args);

    bool should_offer_jobs = true;
    if (!args.empty()) {
        const std::string& first = args.front();
        if (equals_token(first, "-l"))
            should_offer_jobs = false;
    }

    if (should_offer_jobs) {
        auto specs = gather_job_specifiers();
        add_tokens(suggestions, specs);
    }

    return suggestions;
}

std::vector<Suggestion> complete_read(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-r", "-p", "-n", "-d", "-t"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_readonly(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-p", "-f"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_set(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_tokens(suggestions, {"-e", "+e", "-C", "+C", "-u", "+u", "-x", "+x", "-v", "+v", "-n",
                                 "+n", "-f", "+f", "-a", "+a", "-o", "+o", "--"});
        suggestions.push_back({"--errexit-severity=", false});
    }

    const std::string& prev = previous_token(args);
    if (equals_token(prev, "-o") || equals_token(prev, "+o")) {
        add_tokens(suggestions,
                   {"errexit", "noclobber", "nounset", "xtrace", "verbose", "noexec", "noglob",
                    "allexport", "posix", "errexit_severity="},
                   false);
    }

    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_syntax(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_tokens(suggestions, {"-v", "--verbose", "-q", "--quiet", "--no-suggestions",
                                 "--no-context", "--comprehensive", "--semantic", "--style",
                                 "--performance", "--severity", "--category", "-c"});
    }
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_trap(const std::vector<std::string>& args) {
    static const std::vector<std::string> kSignalNames = {
        "EXIT", "ERR",    "DEBUG", "RETURN", "HUP",  "INT",  "QUIT", "ILL",  "TRAP",
        "ABRT", "BUS",    "FPE",   "KILL",   "USR1", "SEGV", "USR2", "PIPE", "ALRM",
        "TERM", "CHLD",   "CONT",  "STOP",   "TSTP", "TTIN", "TTOU", "URG",  "XCPU",
        "XFSZ", "VTALRM", "PROF",  "WINCH",  "IO",   "SYS"};

    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-l", "-p"});

    add_help_flags_if_first(suggestions, args);

    if (completed_arg_count(args) >= 1) {
        const std::string& first = args.front();
        if (!equals_token(first, "-l") && !equals_token(first, "-p"))
            add_tokens(suggestions, kSignalNames);
    }

    return suggestions;
}

std::vector<Suggestion> complete_type(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-a", "-f", "-p", "-t", "-P", "--"}, false);
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_ulimit(const std::vector<std::string>& args) {
    static const std::vector<std::string> kShortOptions = {
        "-a", "-H", "-S", "-b", "-c", "-d", "-e", "-f", "-i", "-l", "-m", "-n",
        "-q", "-r", "-s", "-t", "-u", "-v", "-w", "-y", "-K", "-P", "-T"};
    static const std::vector<std::string> kLongOptions = {"--all",
                                                          "--hard",
                                                          "--soft",
                                                          "--socket-buffers",
                                                          "--core-size",
                                                          "--data-size",
                                                          "--nice",
                                                          "--file-size",
                                                          "--pending-signals",
                                                          "--lock-size",
                                                          "--resident-set-size",
                                                          "--file-descriptor-count",
                                                          "--queue-size",
                                                          "--realtime-priority",
                                                          "--stack-size",
                                                          "--cpu-time",
                                                          "--process-count",
                                                          "--virtual-memory-size",
                                                          "--swap-size",
                                                          "--realtime-maxtime",
                                                          "--kernel-queues",
                                                          "--ptys",
                                                          "--threads"};

    std::vector<Suggestion> suggestions;
    if (is_first_argument(args)) {
        add_tokens(suggestions, kShortOptions, false);
        add_tokens(suggestions, kLongOptions);
    }
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_umask(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-S", "-p", "--version"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_which(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-a", "-s", "--"}, false);
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_pwd(const std::vector<std::string>& args) {
    std::vector<Suggestion> suggestions;
    if (is_first_argument(args))
        add_tokens(suggestions, {"-L", "--logical", "-P", "--physical", "--version"});
    add_help_flags_if_first(suggestions, args);
    return suggestions;
}

std::vector<Suggestion> complete_test_like(const std::vector<std::string>& args, bool extended) {
    static const std::vector<std::string> kUnaryOps = {
        "-n", "-z", "-e", "-f", "-d", "-r", "-w", "-x", "-s", "-L", "-h",
        "-p", "-b", "-c", "-S", "-u", "-g", "-k", "-O", "-G", "-N", "-t"};
    static const std::vector<std::string> kBinaryOps = {
        "=", "==", "!=", "<", ">", "-eq", "-ne", "-lt", "-le", "-gt", "-ge", "-ef", "-nt", "-ot"};

    std::vector<Suggestion> suggestions;
    add_help_flags_if_first(suggestions, args);

    add_tokens(suggestions, kUnaryOps);
    add_tokens(suggestions, kBinaryOps);

    if (extended)
        add_tokens(suggestions, {"=~", "&&", "||", "!"});
    else
        add_tokens(suggestions, {"!", "-a", "-o"});

    return suggestions;
}

}  // namespace

bool add_completions(ic_completion_env_t* cenv, const std::string& command,
                     const std::vector<std::string>& args, bool at_new_token) {
    if (completion_tracker::completion_limit_hit())
        return false;

    const std::string& token = current_token(args);
    std::string command_lower = command;
    std::transform(command_lower.begin(), command_lower.end(), command_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<Suggestion> suggestions;

    if (command_lower == "cjshopt") {
        suggestions = complete_cjshopt(args);
    } else if (command_lower == "hook") {
        suggestions = complete_hook(args);
    } else if (command_lower == "validate") {
        suggestions = complete_validate(args);
    } else if (command_lower == "command") {
        suggestions = complete_command_builtin(args);
    } else if (command_lower == "cjsh-widget") {
        suggestions = complete_cjsh_widget(args);
    } else if (command_lower == "alias") {
        suggestions = complete_alias(args);
    } else if (command_lower == "unalias") {
        suggestions = complete_unalias(args);
    } else if (command_lower == "abbr" || command_lower == "abbreviate") {
        suggestions = complete_abbr(args);
    } else if (command_lower == "unabbr" || command_lower == "unabbreviate") {
        suggestions = complete_unabbr(args);
    } else if (command_lower == "builtin") {
        suggestions = complete_builtin_invocation(args);
    } else if (command_lower == "help") {
        suggestions = complete_builtin_help(args);
    } else if (command_lower == "cd") {
        suggestions = complete_cd(args);
    } else if (command_lower == "echo") {
        suggestions = complete_echo(args);
    } else if (command_lower == "exit" || command_lower == "quit") {
        suggestions = complete_exit(args);
    } else if (command_lower == "fc") {
        suggestions = complete_fc(args);
    } else if (command_lower == "hash") {
        suggestions = complete_hash(args);
    } else if (command_lower == "history") {
        suggestions = complete_history(args);
    } else if (command_lower == "jobs") {
        suggestions = complete_jobs(args);
    } else if (command_lower == "fg" || command_lower == "bg" || command_lower == "wait") {
        suggestions = complete_job_control_target(args);
    } else if (command_lower == "kill") {
        suggestions = complete_kill(args);
    } else if (command_lower == "read") {
        suggestions = complete_read(args);
    } else if (command_lower == "readonly") {
        suggestions = complete_readonly(args);
    } else if (command_lower == "set") {
        suggestions = complete_set(args);
    } else if (command_lower == "syntax") {
        suggestions = complete_syntax(args);
    } else if (command_lower == "trap") {
        suggestions = complete_trap(args);
    } else if (command_lower == "type") {
        suggestions = complete_type(args);
    } else if (command_lower == "ulimit") {
        suggestions = complete_ulimit(args);
    } else if (command_lower == "umask") {
        suggestions = complete_umask(args);
    } else if (command_lower == "which") {
        suggestions = complete_which(args);
    } else if (command_lower == "pwd") {
        suggestions = complete_pwd(args);
    } else if (command_lower == "test" || command_lower == "[") {
        suggestions = complete_test_like(args, false);
    } else if (command_lower == "[[") {
        suggestions = complete_test_like(args, true);
    }

    if (suggestions.empty()) {
        static const std::unordered_set<std::string> kHelpOnlyCommands = {
            "eval",   "exec",   "export", "unset",  "local", "shift",   "break",  "continue",
            "return", "source", ".",      "printf", "times", "getopts", "version"};

        if (kHelpOnlyCommands.find(command_lower) != kHelpOnlyCommands.end())
            add_help_flags_if_first(suggestions, args);
    }

    if (suggestions.empty())
        return false;

    std::unordered_set<std::string> seen;
    bool added_any = false;
    std::string current_prefix = (at_new_token ? std::string() : token);
    long delete_before = (at_new_token ? 0 : static_cast<long>(current_prefix.size()));

    for (const auto& suggestion : suggestions) {
        if (completion_tracker::completion_limit_hit_with_log("builtin argument"))
            break;
        if (ic_stop_completing(cenv))
            break;

        if (!current_prefix.empty() &&
            !completion_utils::matches_completion_prefix(suggestion.text, current_prefix)) {
            continue;
        }

        if (!seen.insert(suggestion.text).second)
            continue;

        std::string insert_text = suggestion.text;
        if (suggestion.append_space && !insert_text.empty() && insert_text.back() != ' ')
            insert_text.push_back(' ');

        if (!completion_tracker::safe_add_completion_prim_with_source(
                cenv, insert_text.c_str(), nullptr, nullptr, kCompletionSource, delete_before, 0)) {
            break;
        }
        added_any = true;
    }

    return added_any;
}

}  // namespace builtin_argument_completion
