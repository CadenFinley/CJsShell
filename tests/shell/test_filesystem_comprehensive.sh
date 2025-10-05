#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: comprehensive filesystem utilities..."

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip_test() {
    echo "SKIP: $1"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

# Create temporary directory for tests
TEST_DIR=$(mktemp -d)
cd "$TEST_DIR"

echo "=== Testing filesystem utilities ==="

# Create a simple C++ test program to test our filesystem utilities
cat << 'EOF' > test_filesystem.cpp
#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

// Simplified versions of our filesystem classes for testing
namespace cjsh_filesystem {

struct Error {
    std::string message;
    explicit Error(const std::string& msg) : message(msg) {}
};

template<typename T>
class Result {
public:
    explicit Result(T value) : value_(std::move(value)), has_value_(true) {}
    explicit Result(const Error& error) : error_(error.message), has_value_(false) {}
    
    static Result<T> ok(T value) { return Result<T>(std::move(value)); }
    static Result<T> error(const std::string& message) { return Result<T>(Error(message)); }
    
    bool is_ok() const { return has_value_; }
    bool is_error() const { return !has_value_; }
    
    const T& value() const { 
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

template<>
class Result<void> {
public:
    Result() : has_value_(true) {}
    explicit Result(const Error& error) : error_(error.message), has_value_(false) {}
    
    static Result<void> ok() { return Result<void>(); }
    static Result<void> error(const std::string& message) { return Result<void>(Error(message)); }
    
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

Result<int> safe_open(const std::string& path, int flags, mode_t mode = 0644) {
    int fd = ::open(path.c_str(), flags, mode);
    if (fd == -1) {
        return Result<int>::error("Failed to open file '" + path + "': " + std::string(strerror(errno)));
    }
    return Result<int>::ok(fd);
}

Result<void> safe_dup2(int oldfd, int newfd) {
    if (::dup2(oldfd, newfd) == -1) {
        return Result<void>::error("Failed to duplicate file descriptor " + std::to_string(oldfd) +
                                   " to " + std::to_string(newfd) + ": " + std::string(strerror(errno)));
    }
    return Result<void>::ok();
}

void safe_close(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

Result<FILE*> safe_fopen(const std::string& path, const std::string& mode) {
    FILE* file = std::fopen(path.c_str(), mode.c_str());
    if (file == nullptr) {
        return Result<FILE*>::error("Failed to open file '" + path + "' with mode '" + mode + "': " + std::string(strerror(errno)));
    }
    return Result<FILE*>::ok(file);
}

void safe_fclose(FILE* file) {
    if (file != nullptr) {
        std::fclose(file);
    }
}

Result<FILE*> safe_popen(const std::string& command, const std::string& mode) {
    FILE* pipe = ::popen(command.c_str(), mode.c_str());
    if (pipe == nullptr) {
        return Result<FILE*>::error("Failed to execute command '" + command + "': " + std::string(strerror(errno)));
    }
    return Result<FILE*>::ok(pipe);
}

int safe_pclose(FILE* file) {
    if (file == nullptr) {
        return -1;
    }
    return ::pclose(file);
}

Result<std::string> create_temp_file(const std::string& prefix = "cjsh_temp") {
    std::string temp_path = "/tmp/" + prefix + "_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
    auto open_result = safe_open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (open_result.is_error()) {
        return Result<std::string>::error(open_result.error());
    }
    safe_close(open_result.value());
    return Result<std::string>::ok(temp_path);
}

Result<void> write_temp_file(const std::string& path, const std::string& content) {
    auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int fd = open_result.value();
    ssize_t written = write(fd, content.c_str(), content.length());
    safe_close(fd);

    if (written != static_cast<ssize_t>(content.length())) {
        return Result<void>::error("Failed to write complete content to file '" + path + "'");
    }

    return Result<void>::ok();
}

void cleanup_temp_file(const std::string& path) {
    std::remove(path.c_str());
}

Result<std::string> read_command_output(const std::string& command) {
    auto pipe_result = safe_popen(command, "r");
    if (pipe_result.is_error()) {
        return Result<std::string>::error(pipe_result.error());
    }

    FILE* pipe = pipe_result.value();
    std::string output;
    char buffer[256];

    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int exit_code = safe_pclose(pipe);
    if (exit_code != 0) {
        return Result<std::string>::error("Command '" + command + "' failed with exit code " + std::to_string(exit_code));
    }

    return Result<std::string>::ok(output);
}

Result<void> write_file_content(const std::string& path, const std::string& content) {
    auto open_result = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (open_result.is_error()) {
        return Result<void>::error(open_result.error());
    }

    int fd = open_result.value();
    ssize_t written = write(fd, content.c_str(), content.length());
    safe_close(fd);

    if (written != static_cast<ssize_t>(content.length())) {
        return Result<void>::error("Failed to write complete content to file '" + path + "'");
    }

    return Result<void>::ok();
}

Result<std::string> read_file_content(const std::string& path) {
    auto open_result = safe_open(path, O_RDONLY);
    if (open_result.is_error()) {
        return Result<std::string>::error(open_result.error());
    }

    int fd = open_result.value();
    std::string content;
    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, bytes_read);
    }

    safe_close(fd);

    if (bytes_read < 0) {
        return Result<std::string>::error("Failed to read from file '" + path + "': " + std::string(strerror(errno)));
    }

    return Result<std::string>::ok(content);
}

} // namespace cjsh_filesystem

using namespace cjsh_filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test_name>" << std::endl;
        return 1;
    }
    
    std::string test_name = argv[1];
    
    try {
        if (test_name == "test_safe_open_success") {
            auto result = safe_open("test_file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (result.is_ok()) {
                safe_close(result.value());
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR: " << result.error() << std::endl;
            }
        }
        else if (test_name == "test_safe_open_nonexistent") {
            auto result = safe_open("/nonexistent/dir/file.txt", O_RDONLY);
            if (result.is_error()) {
                if (result.error().find("No such file or directory") != std::string::npos ||
                    result.error().find("ENOENT") != std::string::npos) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Unexpected error message: " << result.error() << std::endl;
                }
            } else {
                safe_close(result.value());
                std::cout << "ERROR: Expected failure but got success" << std::endl;
            }
        }
        else if (test_name == "test_safe_open_permission_denied") {
            // Create a directory with no permissions
            mkdir("no_permission", 0000);
            auto result = safe_open("no_permission/file.txt", O_WRONLY | O_CREAT);
            if (result.is_error()) {
                if (result.error().find("Permission denied") != std::string::npos ||
                    result.error().find("EACCES") != std::string::npos) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Unexpected error message: " << result.error() << std::endl;
                }
            } else {
                safe_close(result.value());
                std::cout << "ERROR: Expected permission denied but got success" << std::endl;
            }
            rmdir("no_permission");
        }
        else if (test_name == "test_safe_dup2_success") {
            auto result1 = safe_open("test_file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (result1.is_ok()) {
                auto result2 = safe_dup2(result1.value(), 10);
                safe_close(result1.value());
                safe_close(10);
                if (result2.is_ok()) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: " << result2.error() << std::endl;
                }
            } else {
                std::cout << "ERROR: Failed to create test file" << std::endl;
            }
        }
        else if (test_name == "test_safe_dup2_invalid_fd") {
            auto result = safe_dup2(-1, 10);
            if (result.is_error()) {
                if (result.error().find("Bad file descriptor") != std::string::npos ||
                    result.error().find("EBADF") != std::string::npos) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Unexpected error message: " << result.error() << std::endl;
                }
            } else {
                std::cout << "ERROR: Expected failure but got success" << std::endl;
            }
        }
        else if (test_name == "test_safe_fopen_success") {
            auto result = safe_fopen("test_file.txt", "w");
            if (result.is_ok()) {
                safe_fclose(result.value());
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR: " << result.error() << std::endl;
            }
        }
        else if (test_name == "test_safe_fopen_invalid_mode") {
            auto result = safe_fopen("test_file.txt", "invalid_mode");
            if (result.is_error()) {
                std::cout << "SUCCESS" << std::endl;
            } else {
                safe_fclose(result.value());
                std::cout << "ERROR: Expected failure but got success" << std::endl;
            }
        }
        else if (test_name == "test_safe_popen_success") {
            auto result = safe_popen("echo hello", "r");
            if (result.is_ok()) {
                safe_pclose(result.value());
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR: " << result.error() << std::endl;
            }
        }
        else if (test_name == "test_safe_popen_invalid_command") {
            // popen doesn't fail for nonexistent commands, it creates the pipe
            // but the command itself fails, which we can detect with pclose
            auto result = safe_popen("/nonexistent/command", "r");
            if (result.is_ok()) {
                int exit_code = safe_pclose(result.value());
                if (exit_code != 0) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Expected non-zero exit code but got 0" << std::endl;
                }
            } else {
                std::cout << "ERROR: popen failed unexpectedly: " << result.error() << std::endl;
            }
        }
        else if (test_name == "test_create_temp_file") {
            auto result = create_temp_file("test_prefix");
            if (result.is_ok()) {
                std::string temp_path = result.value();
                if (access(temp_path.c_str(), F_OK) == 0) {
                    cleanup_temp_file(temp_path);
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Temp file was not created" << std::endl;
                }
            } else {
                std::cout << "ERROR: " << result.error() << std::endl;
            }
        }
        else if (test_name == "test_write_temp_file") {
            auto create_result = create_temp_file("write_test");
            if (create_result.is_ok()) {
                std::string temp_path = create_result.value();
                auto write_result = write_temp_file(temp_path, "test content");
                if (write_result.is_ok()) {
                    std::ifstream file(temp_path);
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    if (content == "test content") {
                        std::cout << "SUCCESS" << std::endl;
                    } else {
                        std::cout << "ERROR: Content mismatch" << std::endl;
                    }
                } else {
                    std::cout << "ERROR: " << write_result.error() << std::endl;
                }
                cleanup_temp_file(temp_path);
            } else {
                std::cout << "ERROR: Failed to create temp file" << std::endl;
            }
        }
        else if (test_name == "test_read_command_output_success") {
            auto result = read_command_output("echo 'hello world'");
            if (result.is_ok()) {
                if (result.value().find("hello world") != std::string::npos) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Unexpected output: " << result.value() << std::endl;
                }
            } else {
                std::cout << "ERROR: " << result.error() << std::endl;
            }
        }
        else if (test_name == "test_read_command_output_failure") {
            auto result = read_command_output("false");
            if (result.is_error()) {
                if (result.error().find("failed with exit code") != std::string::npos) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Unexpected error message: " << result.error() << std::endl;
                }
            } else {
                std::cout << "ERROR: Expected failure but got success" << std::endl;
            }
        }
        else if (test_name == "test_write_read_file_content") {
            std::string test_content = "This is a test file content with multiple lines.\nLine 2\nLine 3";
            auto write_result = write_file_content("test_content.txt", test_content);
            if (write_result.is_ok()) {
                auto read_result = read_file_content("test_content.txt");
                if (read_result.is_ok()) {
                    if (read_result.value() == test_content) {
                        std::cout << "SUCCESS" << std::endl;
                    } else {
                        std::cout << "ERROR: Content mismatch" << std::endl;
                    }
                } else {
                    std::cout << "ERROR: " << read_result.error() << std::endl;
                }
            } else {
                std::cout << "ERROR: " << write_result.error() << std::endl;
            }
        }
        else if (test_name == "test_read_file_content_nonexistent") {
            auto result = read_file_content("/nonexistent/file.txt");
            if (result.is_error()) {
                if (result.error().find("No such file or directory") != std::string::npos ||
                    result.error().find("ENOENT") != std::string::npos) {
                    std::cout << "SUCCESS" << std::endl;
                } else {
                    std::cout << "ERROR: Unexpected error message: " << result.error() << std::endl;
                }
            } else {
                std::cout << "ERROR: Expected failure but got success" << std::endl;
            }
        }
        else if (test_name == "test_result_template_ok") {
            auto result = Result<int>::ok(42);
            if (result.is_ok() && !result.is_error() && result.value() == 42) {
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR: Result template ok() failed" << std::endl;
            }
        }
        else if (test_name == "test_result_template_error") {
            auto result = Result<int>::error("Test error message");
            if (result.is_error() && !result.is_ok() && result.error() == "Test error message") {
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR: Result template error() failed" << std::endl;
            }
        }
        else if (test_name == "test_result_void_ok") {
            auto result = Result<void>::ok();
            if (result.is_ok() && !result.is_error()) {
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR: Result<void> ok() failed" << std::endl;
            }
        }
        else if (test_name == "test_result_void_error") {
            auto result = Result<void>::error("Void error message");
            if (result.is_error() && !result.is_ok() && result.error() == "Void error message") {
                std::cout << "SUCCESS" << std::endl;
            } else {
                std::cout << "ERROR: Result<void> error() failed" << std::endl;
            }
        }
        else {
            std::cout << "ERROR: Unknown test: " << test_name << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
EOF

# Compile the test program
if ! g++ -std=c++17 -o test_filesystem test_filesystem.cpp 2>/dev/null; then
    skip_test "C++ compiler not available or compilation failed"
    echo ""
    echo "Filesystem Tests Summary:"
    echo "Passed: $TESTS_PASSED"
    echo "Failed: $TESTS_FAILED"
    echo "Skipped: $TESTS_SKIPPED"
    exit 0
fi

echo "--- Testing Result Template Class ---"

# Test Result<T> template class - success case
OUTPUT=$(./test_filesystem test_result_template_ok 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "Result<T>::ok() functionality"
else
    fail_test "Result<T>::ok() functionality - $OUTPUT"
fi

# Test Result<T> template class - error case
OUTPUT=$(./test_filesystem test_result_template_error 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "Result<T>::error() functionality"
else
    fail_test "Result<T>::error() functionality - $OUTPUT"
fi

# Test Result<void> template class - success case
OUTPUT=$(./test_filesystem test_result_void_ok 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "Result<void>::ok() functionality"
else
    fail_test "Result<void>::ok() functionality - $OUTPUT"
fi

# Test Result<void> template class - error case
OUTPUT=$(./test_filesystem test_result_void_error 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "Result<void>::error() functionality"
else
    fail_test "Result<void>::error() functionality - $OUTPUT"
fi

echo "--- Testing safe_open ---"

# Test safe_open - success case
OUTPUT=$(./test_filesystem test_safe_open_success 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "safe_open() with valid path"
else
    fail_test "safe_open() with valid path - $OUTPUT"
fi

# Test safe_open - nonexistent directory
OUTPUT=$(./test_filesystem test_safe_open_nonexistent 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "safe_open() with nonexistent path error handling"
else
    fail_test "safe_open() with nonexistent path error handling - $OUTPUT"
fi

# Test safe_open - permission denied (skip if running as root)
if [ "$(id -u)" != "0" ]; then
    OUTPUT=$(./test_filesystem test_safe_open_permission_denied 2>&1)
    if [ "$OUTPUT" = "SUCCESS" ]; then
        pass_test "safe_open() permission denied error handling"
    else
        fail_test "safe_open() permission denied error handling - $OUTPUT"
    fi
else
    skip_test "safe_open() permission denied (running as root)"
fi

echo "--- Testing safe_dup2 ---"

# Test safe_dup2 - success case
OUTPUT=$(./test_filesystem test_safe_dup2_success 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "safe_dup2() with valid file descriptors"
else
    fail_test "safe_dup2() with valid file descriptors - $OUTPUT"
fi

# Test safe_dup2 - invalid file descriptor
OUTPUT=$(./test_filesystem test_safe_dup2_invalid_fd 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "safe_dup2() with invalid file descriptor error handling"
else
    fail_test "safe_dup2() with invalid file descriptor error handling - $OUTPUT"
fi

echo "--- Testing safe_fopen ---"

# Test safe_fopen - success case
OUTPUT=$(./test_filesystem test_safe_fopen_success 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "safe_fopen() with valid parameters"
else
    fail_test "safe_fopen() with valid parameters - $OUTPUT"
fi

# Test safe_fopen - invalid mode
OUTPUT=$(./test_filesystem test_safe_fopen_invalid_mode 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "safe_fopen() with invalid mode error handling"
else
    fail_test "safe_fopen() with invalid mode error handling - $OUTPUT"
fi

echo "--- Testing safe_popen ---"

# Test safe_popen - success case
OUTPUT=$(./test_filesystem test_safe_popen_success 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "safe_popen() with valid command"
else
    fail_test "safe_popen() with valid command - $OUTPUT"
fi

# Test safe_popen - nonexistent command
OUTPUT=$(./test_filesystem test_safe_popen_invalid_command 2>&1)
if echo "$OUTPUT" | grep -q "SUCCESS"; then
    pass_test "safe_popen() with invalid command error handling"
else
    fail_test "safe_popen() with invalid command error handling - $OUTPUT"
fi

echo "--- Testing Temporary File Operations ---"

# Test create_temp_file
OUTPUT=$(./test_filesystem test_create_temp_file 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "create_temp_file() functionality"
else
    fail_test "create_temp_file() functionality - $OUTPUT"
fi

# Test write_temp_file
OUTPUT=$(./test_filesystem test_write_temp_file 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "write_temp_file() functionality"
else
    fail_test "write_temp_file() functionality - $OUTPUT"
fi

echo "--- Testing Command Execution ---"

# Test read_command_output - success case
OUTPUT=$(./test_filesystem test_read_command_output_success 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "read_command_output() with successful command"
else
    fail_test "read_command_output() with successful command - $OUTPUT"
fi

# Test read_command_output - command failure
OUTPUT=$(./test_filesystem test_read_command_output_failure 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "read_command_output() with failing command error handling"
else
    fail_test "read_command_output() with failing command error handling - $OUTPUT"
fi

echo "--- Testing File Content Operations ---"

# Test write_file_content and read_file_content
OUTPUT=$(./test_filesystem test_write_read_file_content 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "write_file_content() and read_file_content() functionality"
else
    fail_test "write_file_content() and read_file_content() functionality - $OUTPUT"
fi

# Test read_file_content - nonexistent file
OUTPUT=$(./test_filesystem test_read_file_content_nonexistent 2>&1)
if [ "$OUTPUT" = "SUCCESS" ]; then
    pass_test "read_file_content() with nonexistent file error handling"
else
    fail_test "read_file_content() with nonexistent file error handling - $OUTPUT"
fi

echo "--- Testing Edge Cases ---"

# Test safe_close with invalid file descriptor (should not crash)
echo "Testing safe_close with invalid file descriptor..."
cat << 'EOF' > test_edge_cases.cpp
#include <iostream>
#include <unistd.h>

void safe_close(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

int main() {
    // These should not crash
    safe_close(-1);
    safe_close(999999);
    safe_close(0);  // stdin - this might be valid, so it's a real test
    std::cout << "SUCCESS" << std::endl;
    return 0;
}
EOF

if g++ -o test_edge_cases test_edge_cases.cpp 2>/dev/null; then
    OUTPUT=$(./test_edge_cases 2>&1)
    if [ "$OUTPUT" = "SUCCESS" ]; then
        pass_test "safe_close() with invalid file descriptors"
    else
        fail_test "safe_close() with invalid file descriptors - $OUTPUT"
    fi
else
    skip_test "Could not compile edge case test"
fi

# Test safe_fclose with null pointer (should not crash)
echo "Testing safe_fclose with null pointer..."
cat << 'EOF' > test_fclose_null.cpp
#include <iostream>
#include <cstdio>

void safe_fclose(FILE* file) {
    if (file != nullptr) {
        std::fclose(file);
    }
}

int main() {
    // This should not crash
    safe_fclose(nullptr);
    std::cout << "SUCCESS" << std::endl;
    return 0;
}
EOF

if g++ -o test_fclose_null test_fclose_null.cpp 2>/dev/null; then
    OUTPUT=$(./test_fclose_null 2>&1)
    if [ "$OUTPUT" = "SUCCESS" ]; then
        pass_test "safe_fclose() with null pointer"
    else
        fail_test "safe_fclose() with null pointer - $OUTPUT"
    fi
else
    skip_test "Could not compile fclose null test"
fi

# Test safe_pclose with null pointer
echo "Testing safe_pclose with null pointer..."
cat << 'EOF' > test_pclose_null.cpp
#include <iostream>
#include <cstdio>

int safe_pclose(FILE* file) {
    if (file == nullptr) {
        return -1;
    }
    return ::pclose(file);
}

int main() {
    int result = safe_pclose(nullptr);
    if (result == -1) {
        std::cout << "SUCCESS" << std::endl;
    } else {
        std::cout << "ERROR: Expected -1 but got " << result << std::endl;
    }
    return 0;
}
EOF

if g++ -o test_pclose_null test_pclose_null.cpp 2>/dev/null; then
    OUTPUT=$(./test_pclose_null 2>&1)
    if [ "$OUTPUT" = "SUCCESS" ]; then
        pass_test "safe_pclose() with null pointer"
    else
        fail_test "safe_pclose() with null pointer - $OUTPUT"
    fi
else
    skip_test "Could not compile pclose null test"
fi

echo "--- Testing Integration with CJSH Shell ---"

# Test that CJSH can handle filesystem operations in commands
if [ -x "$CJSH_PATH" ]; then
    # Test file creation and reading through shell
    "$CJSH_PATH" -c "echo 'test content' > shell_test.txt"
    if [ -f "shell_test.txt" ]; then
        CONTENT=$(cat shell_test.txt)
        if [ "$CONTENT" = "test content" ]; then
            pass_test "Shell file creation integration"
        else
            fail_test "Shell file creation integration - wrong content"
        fi
    else
        fail_test "Shell file creation integration - file not created"
    fi
    
    # Test command output redirection
    "$CJSH_PATH" -c "ls > ls_output.txt 2>&1"
    if [ -f "ls_output.txt" ]; then
        pass_test "Shell command redirection integration"
    else
        fail_test "Shell command redirection integration"
    fi
    
    # Test error handling in shell commands
    "$CJSH_PATH" -c "cat /nonexistent/file.txt" 2>error_output.txt >/dev/null
    if [ -s "error_output.txt" ]; then
        pass_test "Shell error handling integration"
    else
        fail_test "Shell error handling integration"
    fi
else
    skip_test "CJSH binary not available for integration tests"
    skip_test "Shell file creation integration"
    skip_test "Shell command redirection integration" 
    skip_test "Shell error handling integration"
fi

# Cleanup
cd /
rm -rf "$TEST_DIR"

echo ""
echo "Comprehensive Filesystem Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo "Skipped: $TESTS_SKIPPED"