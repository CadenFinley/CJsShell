#pragma once

#include <limits.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
Result<void> set_close_on_exec(int fd);
Result<void> create_pipe_cloexec(int pipe_fds[2]);
Result<void> duplicate_pipe_read_end_to_fd(int (&pipe_fds)[2], int target_fd);
void close_pipe(int pipe_fds[2]);

Result<void> write_file_content(const std::string& path, const std::string& content);
Result<std::string> read_file_content(const std::string& path);
Result<void> write_all(int fd, std::string_view data);
bool error_indicates_broken_pipe(std::string_view message);

enum class HereStringErrorType : std::uint8_t {
    Pipe,
    Write,
    Dup
};

struct HereStringError {
    HereStringErrorType type;
    std::string detail;
};

std::optional<HereStringError> setup_here_string_stdin(const std::string& here_string);
bool should_noclobber_prevent_overwrite(const std::string& filename, bool force_overwrite = false);
bool command_exists(const std::string& command_path);
bool resolves_to_executable(const std::string& name, const std::string& cwd);
bool path_is_directory_candidate(const std::string& value, const std::string& cwd);

const fs::path& g_user_home_path();

const fs::path& g_cjsh_config_path();

const fs::path& g_cjsh_cache_path();

const fs::path& g_cjsh_profile_path();
const fs::path& g_cjsh_source_path();
const fs::path& g_cjsh_logout_path();

const fs::path& g_cjsh_profile_alt_path();
const fs::path& g_cjsh_source_alt_path();
const fs::path& g_cjsh_logout_alt_path();

const fs::path& g_cjsh_history_path();

const fs::path& g_cjsh_first_boot_path();

const fs::path& g_cjsh_generated_completions_path();

std::vector<std::string> get_executables_in_path();
bool file_exists(const cjsh_filesystem::fs::path& path);
bool initialize_cjsh_directories();
std::string find_executable_in_path(const std::string& name);

bool create_profile_file(const fs::path& target_path = g_cjsh_profile_path());
bool create_source_file(const fs::path& target_path = g_cjsh_source_path());
bool create_logout_file(const fs::path& target_path = g_cjsh_logout_path());

bool init_interactive_filesystem();

bool is_first_boot();
}  // namespace cjsh_filesystem
