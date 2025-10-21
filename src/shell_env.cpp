#include "shell_env.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>

#ifdef __APPLE__
#include <crt_externs.h>
#else
extern "C" char** environ;
#endif

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "shell.h"

namespace {

char** current_environ() {
#if defined(__APPLE__)
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

}  // namespace

namespace cjsh_env {

void setup_environment_variables(const char* argv0) {
    if (argv0 != nullptr) {
        setenv("0", argv0, 1);
    } else {
        setenv("0", "cjsh", 1);
    }

    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);

    if (pw != nullptr) {
        setup_path_variables(pw);

        auto env_vars = setup_user_system_vars(pw);

        for (const auto& [name, value] : env_vars) {
            setenv(name, value, 1);
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

std::vector<std::pair<const char*, const char*>> setup_user_system_vars(const struct passwd* pw) {
    std::vector<std::pair<const char*, const char*>> env_vars;

    env_vars.emplace_back("USER", pw->pw_name);
    env_vars.emplace_back("LOGNAME", pw->pw_name);
    env_vars.emplace_back("HOME", pw->pw_dir);

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        env_vars.emplace_back("HOSTNAME", hostname);
    }

    std::string current_path = std::filesystem::current_path().string();
    std::string shell_path = cjsh_filesystem::get_cjsh_path().string();

    setenv("PWD", current_path.c_str(), 1);
    setenv("SHELL", shell_path.c_str(), 1);
    env_vars.emplace_back("IFS", " \t\n");

    const char* lang_env = getenv("LANG");
    if ((lang_env == nullptr) || lang_env[0] == '\0') {
        env_vars.emplace_back("LANG", "en_US.UTF-8");
    }

    if (getenv("PAGER") == nullptr) {
        env_vars.emplace_back("PAGER", "less");
    }

    if (getenv("TMPDIR") == nullptr) {
        env_vars.emplace_back("TMPDIR", "/tmp");
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

    std::string cjsh_path = cjsh_filesystem::get_cjsh_path().string();
    setenv("_", cjsh_path.c_str(), 1);

    std::string status_str = std::to_string(0);
    setenv("?", status_str.c_str(), 1);

    auto version_str = get_version();
    env_vars.emplace_back("CJSH_VERSION", version_str.c_str());

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
        if (env.first == "PATH") {
            cjsh_filesystem::clear_executable_lookup_cache();
        }
    }
}

std::vector<std::string> parse_shell_command(const std::string& command) {
    std::vector<std::string> args;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (size_t i = 0; i < command.size(); ++i) {
        char c = command[i];

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

std::vector<char*> build_exec_envp(
    const std::vector<std::pair<std::string, std::string>>& env_assignments) {
    static thread_local std::vector<std::unique_ptr<char[]>> env_buffers;
    env_buffers.clear();

    std::unordered_map<std::string, std::string> overrides;
    overrides.reserve(env_assignments.size());
    for (const auto& assignment : env_assignments) {
        overrides[assignment.first] = assignment.second;
    }

    std::vector<char*> envp;
    envp.reserve(overrides.size() + 64);

    char** env = current_environ();
    for (char** current = env; current != nullptr && *current != nullptr; ++current) {
        std::string_view entry(*current);
        size_t equals_pos = entry.find('=');
        if (equals_pos == std::string_view::npos) {
            envp.push_back(*current);
            continue;
        }

        std::string name(entry.substr(0, equals_pos));
        auto it = overrides.find(name);
        if (it == overrides.end()) {
            envp.push_back(*current);
            continue;
        }

        std::string combined = name + "=" + it->second;
        auto buffer = std::make_unique<char[]>(combined.size() + 1);
        std::memcpy(buffer.get(), combined.c_str(), combined.size() + 1);
        envp.push_back(buffer.get());
        env_buffers.push_back(std::move(buffer));
        overrides.erase(it);
    }

    for (const auto& pair : overrides) {
        std::string combined = pair.first + "=" + pair.second;
        auto buffer = std::make_unique<char[]>(combined.size() + 1);
        std::memcpy(buffer.get(), combined.c_str(), combined.size() + 1);
        envp.push_back(buffer.get());
        env_buffers.push_back(std::move(buffer));
    }

    envp.push_back(nullptr);
    return envp;
}

}  // namespace cjsh_env