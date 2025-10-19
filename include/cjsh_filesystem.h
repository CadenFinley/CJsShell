#pragma once

#include <limits.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace cjsh_filesystem {
namespace fs = std::filesystem;

struct Error {
    std::string message;
    explicit Error(const std::string& msg) : message(msg) {
    }
};

template <typename T>
class Result {
   public:
    explicit Result(T value) : value_(std::move(value)), has_value_(true) {
    }
    explicit Result(const Error& error) : error_(error.message), has_value_(false) {
    }

    static Result<T> ok(T value) {
        return Result<T>(std::move(value));
    }
    static Result<T> error(const std::string& message) {
        return Result<T>(Error(message));
    }

    bool is_ok() const {
        return has_value_;
    }
    bool is_error() const {
        return !has_value_;
    }

    const T& value() const {
        if (!has_value_)
            throw std::runtime_error("Attempted to access value of error Result");
        return value_;
    }

    T& value() {
        if (!has_value_)
            throw std::runtime_error("Attempted to access value of error Result");
        return value_;
    }

    const std::string& error() const {
        if (has_value_)
            throw std::runtime_error("Attempted to access error of ok Result");
        return error_;
    }

   private:
    T value_{};
    std::string error_;
    bool has_value_;
};

template <>
class Result<void> {
   public:
    Result() : has_value_(true) {
    }
    explicit Result(const Error& error) : error_(error.message), has_value_(false) {
    }

    static Result<void> ok() {
        return Result<void>();
    }
    static Result<void> error(const std::string& message) {
        return Result<void>(Error(message));
    }

    bool is_ok() const {
        return has_value_;
    }
    bool is_error() const {
        return !has_value_;
    }

    const std::string& error() const {
        if (has_value_)
            throw std::runtime_error("Attempted to access error of ok Result");
        return error_;
    }

   private:
    std::string error_;
    bool has_value_;
};

Result<int> safe_open(const std::string& path, int flags, mode_t mode = 0644);
Result<void> safe_dup2(int oldfd, int newfd);
void safe_close(int fd);
Result<void> redirect_fd(const std::string& file, int target_fd, int flags);

Result<FILE*> safe_fopen(const std::string& path, const std::string& mode);
void safe_fclose(FILE* file);

Result<std::string> create_temp_file(const std::string& prefix = "cjsh_temp");
Result<void> write_temp_file(const std::string& path, const std::string& content);
void cleanup_temp_file(const std::string& path);

Result<void> write_file_content(const std::string& path, const std::string& content);
Result<std::string> read_file_content(const std::string& path);
Result<void> write_all(int fd, std::string_view data);

const fs::path g_user_home_path = []() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
        std::cerr << "Warning: HOME environment variable not set or empty. Using "
                     "/tmp as fallback."
                  << '\n';
        return fs::path("/tmp");
    }
    return fs::path(home);
}();

extern fs::path g_cjsh_path;

const fs::path g_cjsh_profile_path = g_user_home_path / ".cjprofile";

const fs::path g_cjsh_source_path = g_user_home_path / ".cjshrc";

const fs::path g_cjsh_logout_path = g_user_home_path / ".cjsh_logout";

const fs::path g_cache_path = g_user_home_path / ".cache";

const fs::path g_cjsh_cache_path = g_cache_path / "cjsh";

const fs::path g_cjsh_history_path = g_cjsh_cache_path / "history.txt";

const fs::path g_cjsh_found_executables_path = g_cjsh_cache_path / "cached_executables.cache";

const fs::path g_cjsh_path_hash_cache_path = g_cjsh_cache_path / "path_hash.cache";

const fs::path g_cjsh_first_boot_path = g_cjsh_cache_path / ".first_boot";

std::vector<fs::path> read_cached_executables();
bool build_executable_cache();
bool file_exists(const cjsh_filesystem::fs::path& path);
bool should_refresh_executable_cache();
bool initialize_cjsh_path();

void add_executable_to_cache(const std::string& executable_name, const std::string& full_path);
void remove_executable_from_cache(const std::string& executable_name);
void invalidate_executable_cache();
bool is_executable_in_cache(const std::string& executable_name);
void set_last_path_hash(const std::string& path_hash);
std::string get_current_path_hash();
bool has_path_changed();
void cleanup_stale_cache_entries();
void notify_cache_systems_of_update();
bool initialize_cjsh_directories();
std::filesystem::path get_cjsh_path();
std::string find_executable_in_path(const std::string& name);

bool create_profile_file();
bool create_source_file();
bool create_logout_file();

bool init_interactive_filesystem();

bool is_first_boot();
}  
