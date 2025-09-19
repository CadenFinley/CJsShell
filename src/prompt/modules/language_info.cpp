#include "language_info.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

  for (const auto& file : files) {
    if (std::filesystem::exists(current_path / file)) {
      return true;
    }
  }

  for (const auto& folder : folders) {
    if (std::filesystem::exists(current_path / folder) &&
        std::filesystem::is_directory(current_path / folder)) {
      return true;
    }
  }

  try {
    for (const auto& entry :
         std::filesystem::directory_iterator(current_path)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (std::find(extensions.begin(), extensions.end(), ext) !=
            extensions.end()) {
          return true;
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
  // Remove trailing newline if present
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
  return is_project_detected(golang_files, golang_extensions, {});
}

bool LanguageInfo::is_java_project() {
  return is_project_detected(java_files, java_extensions, {});
}

std::string LanguageInfo::get_python_version() {
  std::string output = execute_command(
      "python3 --version 2>/dev/null || python --version 2>/dev/null");
  if (output.empty()) {
    return "";
  }

  std::string version = extract_version(output);
  return version.empty() ? output : version;
}

std::string LanguageInfo::get_nodejs_version() {
  std::string output = execute_command("node --version 2>/dev/null");
  if (output.empty()) {
    return "";
  }

  if (output.front() == 'v') {
    return output.substr(1);
  }

  return output;
}

std::string LanguageInfo::get_rust_version() {
  std::string output = execute_command("rustc --version 2>/dev/null");
  if (output.empty()) {
    return "";
  }

  std::string version = extract_version(output);
  return version.empty() ? output : version;
}

std::string LanguageInfo::get_golang_version() {
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
}

std::string LanguageInfo::get_java_version() {
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
  }

  return false;
}