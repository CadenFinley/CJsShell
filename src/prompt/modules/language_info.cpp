#include "language_info.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <regex>
#include <sstream>

#include "cjsh.h"
#include "utils/cjsh_filesystem.h"

LanguageInfo::LanguageInfo() {
}

bool LanguageInfo::is_project_detected(
    const std::vector<std::string>& files,
    const std::vector<std::string>& extensions,
    const std::vector<std::string>& folders) {
    std::filesystem::path current_path = std::filesystem::current_path();

    
    return scan_directory_recursive(current_path, files, extensions, folders);
}

bool LanguageInfo::scan_directory_recursive(
    const std::filesystem::path& dir, const std::vector<std::string>& files,
    const std::vector<std::string>& extensions,
    const std::vector<std::string>& folders, int max_depth) {
    if (max_depth <= 0) {
        return false;
    }

    try {
        
        for (const auto& file : files) {
            if (std::filesystem::exists(dir / file)) {
                return true;
            }
        }

        
        for (const auto& folder : folders) {
            if (std::filesystem::exists(dir / folder) &&
                std::filesystem::is_directory(dir / folder)) {
                return true;
            }
        }

        
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (std::find(extensions.begin(), extensions.end(), ext) !=
                    extensions.end()) {
                    return true;
                }
            }
        }

        
        
        
        if (max_depth == 3) {
            
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_directory()) {
                    
                    std::string dirname = entry.path().filename().string();
                    if (dirname == "src" || dirname == "lib" ||
                        dirname == "app") {
                        if (scan_directory_recursive(entry.path(), files,
                                                     extensions, folders,
                                                     max_depth - 1)) {
                            return true;
                        }
                    }
                }
            }
        }

    } catch (const std::filesystem::filesystem_error& e) {
        return false;
    }

    return false;
}

std::string LanguageInfo::execute_command(const std::string& command) {
    auto result = cjsh_filesystem::FileOperations::read_command_output(command);
    if (result.is_error()) {
        return "";
    }

    std::string output = result.value();
    
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }

    return output;
}

std::string LanguageInfo::extract_version(const std::string& output) {
    std::regex version_regex("(\\d+\\.\\d+(?:\\.\\d+)?(?:\\S*)?)");
    std::smatch match;

    if (std::regex_search(output, match, version_regex)) {
        return match[1];
    }

    return "";
}

std::string LanguageInfo::get_cached_version(
    const std::string& language_key,
    const std::function<std::string()>& version_func) const {
    std::lock_guard<std::mutex> lock(cache_mutex);

    
    auto it = version_cache.find(language_key);
    if (it != version_cache.end() && it->second.is_valid()) {
        return it->second.version;
    }

    
    std::string version = version_func();

    
    version_cache[language_key] =
        CachedVersion{version, std::chrono::steady_clock::now()};

    return version;
}

bool LanguageInfo::is_python_project() {
    return is_project_detected(python_files, python_extensions, python_folders);
}

bool LanguageInfo::is_nodejs_project() {
    
    return is_project_detected(nodejs_files, nodejs_extensions, nodejs_folders);
}

bool LanguageInfo::is_rust_project() {
    return is_project_detected(rust_files, rust_extensions, rust_folders);
}

bool LanguageInfo::is_golang_project() {
    return is_project_detected(golang_files, golang_extensions, golang_folders);
}

bool LanguageInfo::is_java_project() {
    return is_project_detected(java_files, java_extensions, java_folders);
}

bool LanguageInfo::is_cpp_project() {
    return is_project_detected(cpp_files, cpp_extensions, cpp_folders);
}

bool LanguageInfo::is_csharp_project() {
    return is_project_detected(csharp_files, csharp_extensions, csharp_folders);
}

bool LanguageInfo::is_php_project() {
    return is_project_detected(php_files, php_extensions, php_folders);
}

bool LanguageInfo::is_ruby_project() {
    return is_project_detected(ruby_files, ruby_extensions, ruby_folders);
}

bool LanguageInfo::is_kotlin_project() {
    return is_project_detected(kotlin_files, kotlin_extensions, kotlin_folders);
}

bool LanguageInfo::is_swift_project() {
    return is_project_detected(swift_files, swift_extensions, swift_folders);
}

bool LanguageInfo::is_dart_project() {
    return is_project_detected(dart_files, dart_extensions, dart_folders);
}

bool LanguageInfo::is_scala_project() {
    return is_project_detected(scala_files, scala_extensions, scala_folders);
}

std::string LanguageInfo::get_python_version() {
    return get_cached_version("python", [this]() -> std::string {
        std::string output = execute_command(
            "python3 --version 2>/dev/null || python --version 2>/dev/null");
        if (output.empty()) {
            return "";
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_nodejs_version() {
    return get_cached_version("nodejs", [this]() -> std::string {
        std::string output = execute_command("node --version 2>/dev/null");
        if (output.empty()) {
            return "";
        }

        if (output.front() == 'v') {
            return output.substr(1);
        }

        return output;
    });
}

std::string LanguageInfo::get_rust_version() {
    return get_cached_version("rust", [this]() -> std::string {
        std::string output = execute_command("rustc --version 2>/dev/null");
        if (output.empty()) {
            return "";
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_golang_version() {
    return get_cached_version("golang", [this]() -> std::string {
        std::string output = execute_command("go version 2>/dev/null");
        if (output.empty()) {
            return "";
        }

        std::regex go_version_regex("go(\\d+\\.\\d+(?:\\.\\d+)?)");
        std::smatch match;

        if (std::regex_search(output, match, go_version_regex)) {
            return match[1];
        }

        return "";
    });
}

std::string LanguageInfo::get_java_version() {
    return get_cached_version("java", [this]() -> std::string {
        std::string output = execute_command("java -version 2>&1 | head -n 1");
        if (output.empty()) {
            return "";
        }

        std::regex java_version_regex("\"([^\"]+)\"");
        std::smatch match;

        if (std::regex_search(output, match, java_version_regex)) {
            return match[1];
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_cpp_version() {
    return get_cached_version("cpp", [this]() -> std::string {
        
        std::string output =
            execute_command("g++ --version 2>/dev/null | head -n 1");
        if (output.empty()) {
            output =
                execute_command("clang++ --version 2>/dev/null | head -n 1");
        }
        if (output.empty()) {
            output = execute_command("gcc --version 2>/dev/null | head -n 1");
        }
        if (output.empty()) {
            return "";
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_csharp_version() {
    return get_cached_version("csharp", [this]() -> std::string {
        std::string output = execute_command("dotnet --version 2>/dev/null");
        if (output.empty()) {
            return "";
        }
        return output;
    });
}

std::string LanguageInfo::get_php_version() {
    return get_cached_version("php", [this]() -> std::string {
        std::string output =
            execute_command("php --version 2>/dev/null | head -n 1");
        if (output.empty()) {
            return "";
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_ruby_version() {
    return get_cached_version("ruby", [this]() -> std::string {
        std::string output = execute_command("ruby --version 2>/dev/null");
        if (output.empty()) {
            return "";
        }

        std::regex ruby_version_regex("ruby\\s+(\\d+\\.\\d+\\.\\d+)");
        std::smatch match;

        if (std::regex_search(output, match, ruby_version_regex)) {
            return match[1];
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_kotlin_version() {
    return get_cached_version("kotlin", [this]() -> std::string {
        std::string output = execute_command("kotlin -version 2>&1");
        if (output.empty()) {
            return "";
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_swift_version() {
    return get_cached_version("swift", [this]() -> std::string {
        std::string output = execute_command("swift --version 2>/dev/null");
        if (output.empty()) {
            return "";
        }

        std::regex swift_version_regex(
            "swift-tools-version:(\\d+\\.\\d+)|Swift version "
            "(\\d+\\.\\d+(?:\\.\\d+)?)");
        std::smatch match;

        if (std::regex_search(output, match, swift_version_regex)) {
            return match[1].matched ? match[1] : match[2];
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_dart_version() {
    return get_cached_version("dart", [this]() -> std::string {
        std::string output = execute_command("dart --version 2>&1");
        if (output.empty()) {
            return "";
        }

        std::regex dart_version_regex(
            "Dart SDK version:\\s+(\\d+\\.\\d+\\.\\d+)");
        std::smatch match;

        if (std::regex_search(output, match, dart_version_regex)) {
            return match[1];
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_scala_version() {
    return get_cached_version("scala", [this]() -> std::string {
        std::string output = execute_command("scala -version 2>&1");
        if (output.empty()) {
            return "";
        }

        std::regex scala_version_regex("version\\s+(\\d+\\.\\d+\\.\\d+)");
        std::smatch match;

        if (std::regex_search(output, match, scala_version_regex)) {
            return match[1];
        }

        std::string version = extract_version(output);
        return version.empty() ? output : version;
    });
}

std::string LanguageInfo::get_python_virtual_env() {
    const char* venv = getenv("VIRTUAL_ENV");
    if (venv) {
        std::string venv_path(venv);
        size_t last_slash = venv_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            return venv_path.substr(last_slash + 1);
        } else {
            return venv_path;
        }
    }

    const char* conda_env = getenv("CONDA_DEFAULT_ENV");
    if (conda_env) {
        return std::string(conda_env);
    }

    const char* pipenv = getenv("PIPENV_ACTIVE");
    if (pipenv && std::string(pipenv) == "1") {
        return "pipenv";
    }

    return "";
}

std::string LanguageInfo::get_nodejs_package_manager() {
    std::filesystem::path current_path = std::filesystem::current_path();

    if (std::filesystem::exists(current_path / "yarn.lock")) {
        return "yarn";
    } else if (std::filesystem::exists(current_path / "pnpm-lock.yaml")) {
        return "pnpm";
    } else if (std::filesystem::exists(current_path / "package-lock.json")) {
        return "npm";
    }

    return "npm";
}

std::string LanguageInfo::get_language_version(const std::string& language) {
    if (language == "python") {
        return get_python_version();
    } else if (language == "node" || language == "nodejs") {
        return get_nodejs_version();
    } else if (language == "rust") {
        return get_rust_version();
    } else if (language == "go" || language == "golang") {
        return get_golang_version();
    } else if (language == "java") {
        return get_java_version();
    } else if (language == "cpp" || language == "c++" || language == "c") {
        return get_cpp_version();
    } else if (language == "csharp" || language == "c#" ||
               language == "dotnet") {
        return get_csharp_version();
    } else if (language == "php") {
        return get_php_version();
    } else if (language == "ruby") {
        return get_ruby_version();
    } else if (language == "kotlin") {
        return get_kotlin_version();
    } else if (language == "swift") {
        return get_swift_version();
    } else if (language == "dart") {
        return get_dart_version();
    } else if (language == "scala") {
        return get_scala_version();
    }

    return "";
}

bool LanguageInfo::is_language_project(const std::string& language) {
    if (language == "python") {
        return is_python_project();
    } else if (language == "node" || language == "nodejs") {
        return is_nodejs_project();
    } else if (language == "rust") {
        return is_rust_project();
    } else if (language == "go" || language == "golang") {
        return is_golang_project();
    } else if (language == "java") {
        return is_java_project();
    } else if (language == "cpp" || language == "c++" || language == "c") {
        return is_cpp_project();
    } else if (language == "csharp" || language == "c#" ||
               language == "dotnet") {
        return is_csharp_project();
    } else if (language == "php") {
        return is_php_project();
    } else if (language == "ruby") {
        return is_ruby_project();
    } else if (language == "kotlin") {
        return is_kotlin_project();
    } else if (language == "swift") {
        return is_swift_project();
    } else if (language == "dart") {
        return is_dart_project();
    } else if (language == "scala") {
        return is_scala_project();
    }

    return false;
}

void LanguageInfo::clear_version_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    version_cache.clear();
}