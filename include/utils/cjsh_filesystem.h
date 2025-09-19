#pragma once

#include <limits.h>
#include <unistd.h>

#include <filesystem>
#include <iostream>
#include <vector>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>

// the cjsh file system
namespace cjsh_filesystem {
namespace fs = std::filesystem;

// Error type for Result
struct Error {
    std::string message;
    explicit Error(const std::string& msg) : message(msg) {}
};

// Result template for safe error handling
template<typename T>
class Result {
public:
    explicit Result(T value) : value_(std::move(value)), has_value_(true) {}
    explicit Result(const Error& error) : error_(error.message), has_value_(false) {}
    
    // Factory methods
    static Result<T> ok(T value) { 
        return Result<T>(std::move(value));
    }
    static Result<T> error(const std::string& message) { 
        return Result<T>(Error(message));
    }
    
    bool is_ok() const { return has_value_; }
    bool is_error() const { return !has_value_; }
    
    const T& value() const { 
        if (!has_value_) throw std::runtime_error("Attempted to access value of error Result");
        return value_; 
    }
    
    T& value() { 
        if (!has_value_) throw std::runtime_error("Attempted to access value of error Result");
        return value_; 
    }
    
    const std::string& error() const { 
        if (has_value_) throw std::runtime_error("Attempted to access error of ok Result");
        return error_; 
    }

private:
    T value_{};
    std::string error_;
    bool has_value_;
};

// Specialization for void
template<>
class Result<void> {
public:
    Result() : has_value_(true) {}
    explicit Result(const Error& error) : error_(error.message), has_value_(false) {}
    
    // Factory methods
    static Result<void> ok() { return Result<void>(); }
    static Result<void> error(const std::string& message) { 
        return Result<void>(Error(message));
    }
    
    bool is_ok() const { return has_value_; }
    bool is_error() const { return !has_value_; }
    
    const std::string& error() const { 
        if (has_value_) throw std::runtime_error("Attempted to access error of ok Result");
        return error_; 
    }

private:
    std::string error_;
    bool has_value_;
};

// Safe file operations class
class FileOperations {
public:
    static Result<int> safe_open(const std::string& path, int flags, mode_t mode = 0644);
    static Result<void> safe_dup2(int oldfd, int newfd);
    static void safe_close(int fd);
    static Result<void> redirect_fd(const std::string& file, int target_fd, int flags);
    
    // FILE* based operations
    static Result<FILE*> safe_fopen(const std::string& path, const std::string& mode);
    static void safe_fclose(FILE* file);
    static Result<FILE*> safe_popen(const std::string& command, const std::string& mode);
    static int safe_pclose(FILE* file);
    
    // Temporary file utilities
    static Result<std::string> create_temp_file(const std::string& prefix = "cjsh_temp");
    static Result<void> write_temp_file(const std::string& path, const std::string& content);
    static void cleanup_temp_file(const std::string& path);
    
    // High-level utilities
    static Result<std::string> read_command_output(const std::string& command);
    static Result<void> write_file_content(const std::string& path, const std::string& content);
    static Result<std::string> read_file_content(const std::string& path);
};

// ALL STORED IN FULL PATHS
const fs::path g_user_home_path = []() {
  const char* home = std::getenv("HOME");
  if (!home || home[0] == '\0') {
    std::cerr << "Warning: HOME environment variable not set or empty. Using "
                 "/tmp as fallback."
              << std::endl;
    return fs::path("/tmp");
  }
  return fs::path(home);
}();

// This needs to be non-const because it's initialized at runtime
extern fs::path g_cjsh_path;

// used if login
const fs::path g_cjsh_profile_path =
    g_user_home_path /
    ".cjprofile";  // envvars loaded on login shell also startup flags

// used if interactive
const fs::path g_cjsh_source_path =
    g_user_home_path / ".cjshrc";  // aliases, prompt, functions, themes loaded
                                   // on interactive shell

const fs::path g_config_path =
    g_user_home_path / ".config";                           // config directory
const fs::path g_cache_path = g_user_home_path / ".cache";  // cache directory

const fs::path g_cjsh_data_path =
    g_config_path / "cjsh";  // directory for all cjsh things
const fs::path g_cjsh_cache_path =
    g_cache_path / "cjsh";  // cache directory for cjsh

const fs::path g_cjsh_plugin_path =
    g_cjsh_data_path / "plugins";  // where all plugins are stored
const fs::path g_cjsh_theme_path =
    g_cjsh_data_path / "themes";  // where all themes are stored
const fs::path g_cjsh_history_path =
    g_cjsh_cache_path / "history.txt";  // where the history is stored

const fs::path g_cjsh_ai_config_path =
    g_cjsh_data_path / "ai";  // where the ai config is stored
const fs::path g_cjsh_ai_config_file_path =
    g_cjsh_ai_config_path / "config.json";  // where the ai config is stored
const fs::path g_cjsh_ai_default_config_path =
    g_cjsh_ai_config_path / "default.json";  // default ai config

const fs::path g_cjsh_ai_conversations_path =
    g_cjsh_cache_path /
    "conversations";  // where the ai conversations are stored

const fs::path g_cjsh_found_executables_path =
    g_cjsh_cache_path /
    "cached_executables.cache";  // where the found executables are stored for
                                 // syntax highlighting and completions

std::vector<fs::path> read_cached_executables();
bool build_executable_cache();
bool file_exists(const cjsh_filesystem::fs::path& path);
bool should_refresh_executable_cache();
bool initialize_cjsh_path();
bool initialize_cjsh_directories();
std::filesystem::path get_cjsh_path();
std::string find_executable_in_path(const std::string& name);
}  // namespace cjsh_filesystem
