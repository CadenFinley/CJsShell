#include "directory_info.h"

#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <vector>

bool use_logical_path = false;
bool truncate_to_repo = true;
int truncation_length = 3;
std::string truncation_symbol = "…";
std::string home_symbol = "~";
std::unordered_map<std::string, std::string> substitutions;

std::string get_display_directory() {
    std::filesystem::path current_dir = std::filesystem::current_path();
    std::filesystem::path home_dir;

    const char* home = getenv("HOME");
    if (home) {
        home_dir = std::filesystem::path(home);
    }

    std::filesystem::path repo_root;
    bool is_git_repo = false;
    std::filesystem::path temp_path = current_dir;

    while (temp_path != temp_path.root_path()) {
        if (std::filesystem::exists(temp_path / ".git")) {
            repo_root = temp_path;
            is_git_repo = true;
            break;
        }
        temp_path = temp_path.parent_path();
    }

    std::string dir_string;

    if (truncate_to_repo && is_git_repo && repo_root != home_dir) {
        dir_string = contract_repo_path(current_dir, repo_root);
    } else {
        dir_string = contract_path(current_dir, home_dir, home_symbol);
    }

    dir_string = substitute_path(dir_string, substitutions);

    dir_string = truncate_path(dir_string, truncation_length);

    return dir_string;
}

std::string get_directory_name() {
    std::filesystem::path current_dir = std::filesystem::current_path();
    return current_dir.filename().string();
}

std::string get_truncated_path() {
    return get_display_directory();
}

std::string get_repo_relative_path(const std::filesystem::path& repo_root) {
    std::filesystem::path current_dir = std::filesystem::current_path();
    return contract_repo_path(current_dir, repo_root);
}

bool is_truncated() {
    std::string full_path = std::filesystem::current_path().string();
    std::string display_path = get_display_directory();
    return full_path.length() != display_path.length();
}

std::string contract_path(const std::filesystem::path& path, const std::filesystem::path& home_dir,
                          const std::string& home_symbol) {
    std::string path_str = path.string();
    std::string home_str = home_dir.string();

    if (path_str == home_str) {
        return home_symbol;
    }

    if (path_str.length() >= home_str.length() + 1 &&
        path_str.substr(0, home_str.length() + 1) == home_str + "/") {
        return home_symbol + path_str.substr(home_str.length());
    }

    return path_str;
}

std::string contract_repo_path(const std::filesystem::path& path,
                               const std::filesystem::path& repo_root) {
    std::string path_str = path.string();
    std::string repo_str = repo_root.string();

    if (path_str == repo_str) {
        return repo_root.filename().string();
    }

    if (path_str.length() >= repo_str.length() + 1 &&
        path_str.substr(0, repo_str.length() + 1) == repo_str + "/") {
        std::string relative = path_str.substr(repo_str.length() + 1);
        return repo_root.filename().string() + "/" + relative;
    }

    return path_str;
}

std::string substitute_path(const std::string& path,
                            const std::unordered_map<std::string, std::string>& substitutions) {
    std::string result = path;

    for (const auto& [from, to] : substitutions) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    return result;
}

std::string truncate_path(const std::string& path, int max_length) {
    if (max_length <= 0) {
        return path;
    }

    std::vector<std::string> components;
    std::stringstream ss(path);
    std::string component;

    while (std::getline(ss, component, '/')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }

    if (static_cast<int>(components.size()) <= max_length) {
        return path;
    }

    std::string result = truncation_symbol;
    for (int i = components.size() - max_length; i < static_cast<int>(components.size()); ++i) {
        result += "/" + components[i];
    }

    return result;
}

bool is_readonly_dir(const std::filesystem::path& path) {
    return access(path.c_str(), W_OK) != 0;
}

void set_use_logical_path(bool use_logical) {
    use_logical_path = use_logical;
}

void set_truncate_to_repo(bool truncate) {
    truncate_to_repo = truncate;
}

void set_truncation_length(int length) {
    truncation_length = length;
}

void set_home_symbol(const std::string& symbol) {
    home_symbol = symbol;
}

void add_substitution(const std::string& from, const std::string& to) {
    substitutions[from] = to;
}