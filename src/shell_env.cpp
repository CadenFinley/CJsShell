#include "shell_env.h"

#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "prompt/prompt.h"
#include "shell.h"

namespace cjsh_env {

void setup_environment_variables(const char* argv0) {
    std::string shell_value = "cjsh";
    std::string existing_shell_value;
    if (const char* existing_shell_env = getenv("SHELL");
        existing_shell_env != nullptr && existing_shell_env[0] != '\0') {
        existing_shell_value = existing_shell_env;
        shell_value = existing_shell_value;
    }

    std::string candidate_shell;

    if (argv0 != nullptr) {
        setenv("0", argv0, 1);
        std::string argv0_str(argv0);
        if (!argv0_str.empty() && argv0_str.front() == '-') {
            argv0_str.erase(argv0_str.begin());
        }
        if (!argv0_str.empty()) {
            if (argv0_str.find('/') != std::string::npos) {
                std::filesystem::path path_candidate(argv0_str);
                if (!path_candidate.is_absolute()) {
                    path_candidate = std::filesystem::absolute(path_candidate);
                }
                candidate_shell = path_candidate.lexically_normal().string();
            } else {
                std::string resolved_shell = cjsh_filesystem::find_executable_in_path(argv0_str);
                if (!resolved_shell.empty()) {
                    candidate_shell = resolved_shell;
                } else {
                    candidate_shell = argv0_str;
                }
            }
        }
    } else {
        setenv("0", "cjsh", 1);
    }

    if (candidate_shell.empty() && !existing_shell_value.empty() &&
        existing_shell_value.front() == '-') {
        candidate_shell = existing_shell_value.substr(1);
    }

    if (!candidate_shell.empty()) {
        bool existing_invalid = existing_shell_value.empty() ||
                                existing_shell_value.front() == '-' || shell_value == "cjsh";
        if (existing_invalid) {
            shell_value = candidate_shell;
        }
    }

    if (!shell_value.empty() && shell_value.front() == '-') {
        shell_value.erase(shell_value.begin());
    }
    if (shell_value.empty()) {
        shell_value = "cjsh";
    }

    setenv("SHELL", shell_value.c_str(), 1);
    setenv("_", shell_value.c_str(), 1);

    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);

    if (pw != nullptr) {
        setup_path_variables(pw);

        auto env_vars = setup_user_system_vars(pw);

        for (const auto& [name, value] : env_vars) {
            setenv(name.c_str(), value.c_str(), 1);
        }
    }
}

void setup_path_variables(const struct passwd* pw) {
    const char* path_env = getenv("PATH");
    if ((path_env == nullptr) || path_env[0] == '\0') {
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }

#ifdef __APPLE__

    (void)pw;

    if (config::login_mode && cjsh_filesystem::file_exists("/usr/libexec/path_helper")) {
        if (g_shell) {
            int result = g_shell->execute("eval \"$(/usr/libexec/path_helper -s)\"");
            (void)result;
        }
    }
#endif

#ifdef __linux__
    if (path_env && path_env[0] != '\0') {
        std::string current_path = path_env;
        std::vector<std::string> additional_paths;

        std::string home_bin = std::string(pw->pw_dir) + "/bin";
        std::string home_local_bin = std::string(pw->pw_dir) + "/.local/bin";

        std::vector<std::string> system_paths = {
            "/usr/local/sbin", "/snap/bin",   "/var/lib/snapd/snap/bin", "/opt/bin", "/usr/games",
            home_bin,          home_local_bin};

        for (const auto& path : system_paths) {
            if (cjsh_filesystem::file_exists(path)) {
                if (current_path.find(path) == std::string::npos) {
                    additional_paths.push_back(path);
                }
            }
        }

        if (!additional_paths.empty()) {
            std::string new_path;
            for (const auto& path : additional_paths) {
                if (!new_path.empty())
                    new_path += ":";
                new_path += path;
            }
            new_path += ":" + current_path;
            setenv("PATH", new_path.c_str(), 1);
        }

        if (getenv("MANPATH") == nullptr) {
            std::vector<std::string> manpaths = {"/usr/local/man", "/usr/local/share/man",
                                                 "/usr/share/man", "/usr/man"};

            std::string manpath_str;
            for (const auto& path : manpaths) {
                if (cjsh_filesystem::file_exists(path)) {
                    if (!manpath_str.empty())
                        manpath_str += ":";
                    manpath_str += path;
                }
            }

            if (!manpath_str.empty()) {
                setenv("MANPATH", manpath_str.c_str(), 1);
            }
        }
    }
#endif
}

std::vector<std::pair<std::string, std::string>> setup_user_system_vars(const struct passwd* pw) {
    std::vector<std::pair<std::string, std::string>> env_vars;

    env_vars.emplace_back("USER", std::string(pw->pw_name));
    env_vars.emplace_back("LOGNAME", std::string(pw->pw_name));
    env_vars.emplace_back("HOME", std::string(pw->pw_dir));

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        env_vars.emplace_back("HOSTNAME", std::string(hostname));
    }

    std::string current_path = std::filesystem::current_path().string();

    setenv("PWD", current_path.c_str(), 1);
    env_vars.emplace_back("IFS", std::string(" \t\n"));

    const char* lang_env = getenv("LANG");
    if ((lang_env == nullptr) || lang_env[0] == '\0') {
        env_vars.emplace_back("LANG", std::string("en_US.UTF-8"));
    }

    if (getenv("PAGER") == nullptr) {
        env_vars.emplace_back("PAGER", std::string("less"));
    }

    if (getenv("TMPDIR") == nullptr) {
        env_vars.emplace_back("TMPDIR", std::string("/tmp"));
    }

    int shlvl = 1;
    if (const char* current_shlvl = getenv("SHLVL")) {
        try {
            shlvl = std::stoi(current_shlvl) + 1;
        } catch (...) {
            shlvl = 1;
        }
    }
    std::string shlvl_str = std::to_string(shlvl);
    setenv("SHLVL", shlvl_str.c_str(), 1);

    std::string status_str = std::to_string(0);
    setenv("?", status_str.c_str(), 1);

    auto version_str = get_version();
    env_vars.emplace_back("CJSH_VERSION", version_str);

    if (getenv("PS1") == nullptr) {
        std::string default_ps1 = prompt::default_primary_prompt_template();
        setenv("PS1", default_ps1.c_str(), 1);
    }

    return env_vars;
}

bool is_valid_env_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    unsigned char first = static_cast<unsigned char>(name[0]);
    if ((std::isalpha(first) == 0) && first != '_') {
        return false;
    }
    if (!std::all_of(name.begin(), name.end(), [](char c) {
            unsigned char ch = static_cast<unsigned char>(c);
            return (std::isalnum(ch) != 0) || ch == '_';
        })) {
        return false;
    }
    return true;
}

size_t collect_env_assignments(const std::vector<std::string>& args,
                               std::vector<std::pair<std::string, std::string>>& env_assignments) {
    size_t cmd_start_idx = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];
        size_t pos = token.find('=');
        if (pos != std::string::npos && pos > 0) {
            std::string name = token.substr(0, pos);
            if (is_valid_env_name(name)) {
                env_assignments.push_back({name, token.substr(pos + 1)});
                cmd_start_idx = i + 1;
                continue;
            }
        }
        break;
    }
    return cmd_start_idx;
}

void apply_env_assignments(
    const std::vector<std::pair<std::string, std::string>>& env_assignments) {
    for (const auto& env : env_assignments) {
        setenv(env.first.c_str(), env.second.c_str(), 1);
    }
}

std::vector<std::string> parse_shell_command(const std::string& command) {
    std::vector<std::string> args;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (char c : command) {
        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\' && !in_single_quote) {
            escaped = true;
            continue;
        }

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }

        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }

        if ((c == ' ' || c == '\t') && !in_single_quote && !in_double_quote) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty()) {
        args.push_back(current);
    }

    return args;
}

std::vector<char*> build_exec_argv(const std::vector<std::string>& args) {
    static thread_local std::vector<std::unique_ptr<char[]>> arg_buffers;
    arg_buffers.clear();

    std::vector<char*> c_args;
    c_args.reserve(args.size() + 1);
    for (const auto& arg : args) {
        auto buf = std::make_unique<char[]>(arg.size() + 1);
        std::memcpy(buf.get(), arg.c_str(), arg.size() + 1);
        c_args.push_back(buf.get());
        arg_buffers.push_back(std::move(buf));
    }
    c_args.push_back(nullptr);
    return c_args;
}

bool update_terminal_dimensions() {
    struct winsize ws{};

    const int fds[] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
    bool updated = false;

    for (int fd : fds) {
        if (fd < 0) {
            continue;
        }

        if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && (ws.ws_col > 0 || ws.ws_row > 0)) {
            updated = true;
            break;
        }
    }

    if (!updated) {
        return false;
    }

    if (ws.ws_col > 0) {
        std::string columns = std::to_string(ws.ws_col);
        setenv("COLUMNS", columns.c_str(), 1);
    }

    if (ws.ws_row > 0) {
        std::string lines = std::to_string(ws.ws_row);
        setenv("LINES", lines.c_str(), 1);
    }

    return updated;
}

}  // namespace cjsh_env