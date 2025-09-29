#include "env.h"

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
    // setup essential environment variables for the shell session
    if (g_debug_mode)
        std::cerr << "DEBUG: Setting up environment variables" << std::endl;

    // Set shell environment variable ($0)
    if (argv0) {
        if (g_debug_mode)
            std::cerr << "DEBUG: Setting $0=" << argv0 << std::endl;
        setenv("0", argv0, 1);
    } else {
        if (g_debug_mode)
            std::cerr << "DEBUG: Setting $0=cjsh" << std::endl;
        setenv("0", "cjsh", 1);
    }

    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);

    if (pw != nullptr) {
        // Setup PATH variables first (platform-specific)
        setup_path_variables(pw);

        // Get user and system variables
        auto env_vars = setup_user_system_vars(pw);

        // Set all environment variables in one batch
        if (g_debug_mode) {
            std::cerr << "DEBUG: Setting " << env_vars.size()
                      << " environment variables" << std::endl;
        }

        // Actually set the environment variables
        for (const auto& [name, value] : env_vars) {
            setenv(name, value, 1);
        }
    }
}

void setup_path_variables(const struct passwd* pw) {
    // Check PATH and add if needed
    const char* path_env = getenv("PATH");
    if (!path_env || path_env[0] == '\0') {
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }

// On macOS, use path_helper to set up standard paths (only for login shells)
#ifdef __APPLE__
    // pw parameter is unused on macOS since we use path_helper
    (void)pw;

    if (config::login_mode &&
        cjsh_filesystem::file_exists("/usr/libexec/path_helper")) {
        if (g_debug_mode) {
            std::cerr << "DEBUG: Running /usr/libexec/path_helper via shell"
                      << std::endl;
        }

        std::string old_path = getenv("PATH") ? getenv("PATH") : "";
        std::string old_manpath = getenv("MANPATH") ? getenv("MANPATH") : "";

        if (!g_shell) {
            if (g_debug_mode) {
                std::cerr << "DEBUG: Shell not available for path_helper"
                          << std::endl;
            }
        } else {
            int result =
                g_shell->execute("eval \"$(/usr/libexec/path_helper -s)\"");

            if (result == 0) {
                const char* new_path = getenv("PATH");
                if (new_path && std::string(new_path) != old_path &&
                    g_debug_mode) {
                    std::cerr
                        << "DEBUG: PATH updated via path_helper: " << new_path
                        << std::endl;
                }

                const char* new_manpath = getenv("MANPATH");
                if (new_manpath && std::string(new_manpath) != old_manpath &&
                    g_debug_mode) {
                    std::cerr << "DEBUG: MANPATH updated via path_helper: "
                              << new_manpath << std::endl;
                }
            } else if (g_debug_mode) {
                std::cerr
                    << "DEBUG: path_helper execution failed with exit code "
                    << result << std::endl;
            }
        }
    }
#endif

// On Linux, augment PATH with common additional directories
#ifdef __linux__
    if (path_env && path_env[0] != '\0') {
        std::string current_path = path_env;
        std::vector<std::string> additional_paths;

        // Common user-local directories
        std::string home_bin = std::string(pw->pw_dir) + "/bin";
        std::string home_local_bin = std::string(pw->pw_dir) + "/.local/bin";

        // Check for common additional system directories
        std::vector<std::string> system_paths = {
            "/usr/local/sbin",
            "/snap/bin",                // Ubuntu/Debian snap packages
            "/var/lib/snapd/snap/bin",  // Alternative snap location
            "/opt/bin",                 // Optional packages
            "/usr/games",               // Games (some distros)
            home_bin,                   // User's personal bin
            home_local_bin              // User's .local/bin (Python pip, etc.)
        };

        for (const auto& path : system_paths) {
            if (cjsh_filesystem::file_exists(path)) {
                // Check if path is already in PATH
                if (current_path.find(path) == std::string::npos) {
                    additional_paths.push_back(path);
                    if (g_debug_mode) {
                        std::cerr << "DEBUG: Adding to PATH: " << path
                                  << std::endl;
                    }
                }
            }
        }

        // Prepend additional paths to maintain priority order
        if (!additional_paths.empty()) {
            std::string new_path;
            for (const auto& path : additional_paths) {
                if (!new_path.empty())
                    new_path += ":";
                new_path += path;
            }
            new_path += ":" + current_path;
            setenv("PATH", new_path.c_str(), 1);

            if (g_debug_mode) {
                std::cerr << "DEBUG: Updated PATH on Linux: " << new_path
                          << std::endl;
            }
        }

        // Set up MANPATH if not already set
        if (getenv("MANPATH") == nullptr) {
            std::vector<std::string> manpaths = {"/usr/local/man",
                                                 "/usr/local/share/man",
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
                if (g_debug_mode) {
                    std::cerr << "DEBUG: Set MANPATH on Linux: " << manpath_str
                              << std::endl;
                }
            }
        }
    }
#endif
}

std::vector<std::pair<const char*, const char*>> setup_user_system_vars(
    const struct passwd* pw) {
    std::vector<std::pair<const char*, const char*>> env_vars;

    // User info variables
    env_vars.emplace_back("USER", pw->pw_name);
    env_vars.emplace_back("LOGNAME", pw->pw_name);
    env_vars.emplace_back("HOME", pw->pw_dir);

    // System info
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        env_vars.emplace_back("HOSTNAME", hostname);
    }

    // Current directory and shell info
    std::string current_path = std::filesystem::current_path().string();
    std::string shell_path = cjsh_filesystem::get_cjsh_path().string();

    if (g_debug_mode) {
        std::cerr << "DEBUG: Setting SHELL to: " << shell_path << std::endl;
    }

    // Set environment variables directly instead of storing in vector with
    // dangling pointers
    setenv("PWD", current_path.c_str(), 1);
    setenv("SHELL", shell_path.c_str(), 1);
    env_vars.emplace_back("IFS", " \t\n");

    // Language settings
    const char* lang_env = getenv("LANG");
    if (!lang_env || lang_env[0] == '\0') {
        env_vars.emplace_back("LANG", "en_US.UTF-8");
    }

    // Optional variables
    if (getenv("PAGER") == nullptr) {
        env_vars.emplace_back("PAGER", "less");
    }

    if (getenv("TMPDIR") == nullptr) {
        env_vars.emplace_back("TMPDIR", "/tmp");
    }

    // Shell level - set directly to avoid dangling pointer
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
    if (g_debug_mode) {
        std::cerr << "DEBUG: Setting SHLVL to: " << shlvl_str << std::endl;
    }

    // Miscellaneous - set directly to avoid dangling pointer
    std::string cjsh_path = cjsh_filesystem::get_cjsh_path().string();
    if (g_debug_mode) {
        std::cerr << "DEBUG: Setting _ to: " << cjsh_path << std::endl;
    }
    setenv("_", cjsh_path.c_str(), 1);

    // Use bash-like exit status variable instead of STATUS - set directly
    std::string status_str = std::to_string(0);
    setenv("?", status_str.c_str(), 1);

    // Set shell-specific version variable (optional)
    env_vars.emplace_back("CJSH_VERSION", c_version.c_str());

    return env_vars;
}

}  // namespace cjsh_env