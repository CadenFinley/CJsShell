#include "basic_info.h"

#include <pwd.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>

bool is_root_path(const std::filesystem::path& path) {
    return path == path.root_path();
}

std::string get_current_file_path() {
    std::string path = std::filesystem::current_path().string();

    if (path == "/") {
        return "/";
    }

    char* home_dir = getenv("HOME");
    if (home_dir) {
        std::string home_str(home_dir);
        if (path.length() >= home_str.length() && path.substr(0, home_str.length()) == home_str) {
            if (path == home_str) {
                path = "~";
            } else {
                path = "~" + path.substr(home_str.length());
            }
        }
    }
    return path;
}

std::string get_current_file_name() {
    std::filesystem::path current_path = std::filesystem::current_path();
    std::string filename = current_path.filename().string();

    if (filename.empty()) {
        filename = current_path.string();
    }

    char* home_dir = getenv("HOME");
    if (home_dir) {
        std::string home_str(home_dir);
        std::string current_str = current_path.string();

        if (current_str == home_str) {
            filename = "~";
        } else if (current_str.length() > home_str.length() &&
                   current_str.substr(0, home_str.length()) == home_str &&
                   current_str[home_str.length()] == '/') {
            filename = current_path.filename().string();
        }
    }

    return filename;
}

std::string get_username() {
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        return std::string(pw->pw_name);
    }
    return "unknown";
}

std::string get_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "unknown";
}
