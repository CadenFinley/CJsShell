#include "shell_env.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <iostream>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "shell.h"

namespace cjsh_env {

void setup_environment_variables(const char* argv0) {
    if (argv0) {
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
    if (!path_env || path_env[0] == '\0') {
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }

#ifdef __APPLE__

    (void)pw;

    if (config::login_mode && cjsh_filesystem::file_exists("/usr/libexec/path_helper")) {
        std::string old_path = getenv("PATH") ? getenv("PATH") : "";
        std::string old_manpath = getenv("MANPATH") ? getenv("MANPATH") : "";

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
    if (!lang_env || lang_env[0] == '\0') {
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

}  // namespace cjsh_env