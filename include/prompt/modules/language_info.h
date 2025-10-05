#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct CachedVersion {
    std::string version;
    std::chrono::steady_clock::time_point timestamp;
    bool is_valid() const {
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);
        return age.count() < 300;
    }
};

extern const std::vector<std::string> python_files;
extern const std::vector<std::string> python_extensions;
extern const std::vector<std::string> python_folders;

extern const std::vector<std::string> nodejs_files;
extern const std::vector<std::string> nodejs_extensions;
extern const std::vector<std::string> nodejs_folders;

extern const std::vector<std::string> rust_files;
extern const std::vector<std::string> rust_extensions;
extern const std::vector<std::string> rust_folders;

extern const std::vector<std::string> golang_files;
extern const std::vector<std::string> golang_extensions;
extern const std::vector<std::string> golang_folders;

extern const std::vector<std::string> java_files;
extern const std::vector<std::string> java_extensions;
extern const std::vector<std::string> java_folders;

extern const std::vector<std::string> cpp_files;
extern const std::vector<std::string> cpp_extensions;
extern const std::vector<std::string> cpp_folders;

extern const std::vector<std::string> csharp_files;
extern const std::vector<std::string> csharp_extensions;
extern const std::vector<std::string> csharp_folders;

extern const std::vector<std::string> php_files;
extern const std::vector<std::string> php_extensions;
extern const std::vector<std::string> php_folders;

extern const std::vector<std::string> ruby_files;
extern const std::vector<std::string> ruby_extensions;
extern const std::vector<std::string> ruby_folders;

extern const std::vector<std::string> kotlin_files;
extern const std::vector<std::string> kotlin_extensions;
extern const std::vector<std::string> kotlin_folders;

extern const std::vector<std::string> swift_files;
extern const std::vector<std::string> swift_extensions;
extern const std::vector<std::string> swift_folders;

extern const std::vector<std::string> dart_files;
extern const std::vector<std::string> dart_extensions;
extern const std::vector<std::string> dart_folders;

extern const std::vector<std::string> scala_files;
extern const std::vector<std::string> scala_extensions;
extern const std::vector<std::string> scala_folders;

bool is_project_detected(const std::vector<std::string>& files,
                         const std::vector<std::string>& extensions,
                         const std::vector<std::string>& folders);
bool scan_directory_recursive(const std::filesystem::path& dir,
                              const std::vector<std::string>& files,
                              const std::vector<std::string>& extensions,
                              const std::vector<std::string>& folders, int max_depth = 3);
std::string extract_version(const std::string& output);

std::string get_cached_version(const std::string& language_key,
                               const std::function<std::string()>& version_func);

bool is_python_project();
bool is_nodejs_project();
bool is_rust_project();
bool is_golang_project();
bool is_java_project();
bool is_cpp_project();
bool is_csharp_project();
bool is_php_project();
bool is_ruby_project();
bool is_kotlin_project();
bool is_swift_project();
bool is_dart_project();
bool is_scala_project();

std::string get_python_version();
std::string get_nodejs_version();
std::string get_rust_version();
std::string get_golang_version();
std::string get_java_version();
std::string get_cpp_version();
std::string get_csharp_version();
std::string get_php_version();
std::string get_ruby_version();
std::string get_kotlin_version();
std::string get_swift_version();
std::string get_dart_version();
std::string get_scala_version();

std::string get_python_virtual_env();
std::string get_nodejs_package_manager();

std::string get_language_version(const std::string& language);
bool is_language_project(const std::string& language);

void clear_version_cache();
