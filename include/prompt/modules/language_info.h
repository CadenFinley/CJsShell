#pragma once

#include <filesystem>
#include <string>
#include <vector>

class LanguageInfo {
 private:
  // File detection patterns
  std::vector<std::string> python_files = {
      "requirements.txt", "requirements-dev.txt",
      "pyproject.toml",   "Pipfile",
      "Pipfile.lock",     "setup.py",
      "setup.cfg",        "tox.ini",
      ".python-version",  "environment.yml",
      "conda.yml"};
  std::vector<std::string> python_extensions = {".py", ".pyi", ".pyx"};
  std::vector<std::string> python_folders = {"__pycache__", ".venv", "venv",
                                             "env"};

  std::vector<std::string> nodejs_files = {"package.json", "package-lock.json",
                                           "yarn.lock",    ".node-version",
                                           ".nvmrc",       "pnpm-lock.yaml"};
  std::vector<std::string> nodejs_extensions = {".js", ".mjs", ".cjs",
                                                ".ts", ".tsx", ".vue"};
  std::vector<std::string> nodejs_folders = {"node_modules"};

  std::vector<std::string> rust_files = {"Cargo.toml", "Cargo.lock"};
  std::vector<std::string> rust_extensions = {".rs"};
  std::vector<std::string> rust_folders = {"target"};

  std::vector<std::string> golang_files = {
      "go.mod",     "go.sum",     "Gopkg.toml", "Gopkg.lock",
      "glide.yaml", "glide.lock", "Godeps"};
  std::vector<std::string> golang_extensions = {".go"};

  std::vector<std::string> java_files = {"pom.xml", "build.gradle",
                                         "build.gradle.kts", "build.xml",
                                         "gradle.properties"};
  std::vector<std::string> java_extensions = {".java", ".class", ".jar"};

  bool is_project_detected(const std::vector<std::string>& files,
                           const std::vector<std::string>& extensions,
                           const std::vector<std::string>& folders);
  std::string execute_command(const std::string& command);
  std::string extract_version(const std::string& output);

 public:
  LanguageInfo();

  bool is_python_project();
  bool is_nodejs_project();
  bool is_rust_project();
  bool is_golang_project();
  bool is_java_project();

  std::string get_python_version();
  std::string get_nodejs_version();
  std::string get_rust_version();
  std::string get_golang_version();
  std::string get_java_version();

  std::string get_python_virtual_env();
  std::string get_nodejs_package_manager();

  std::string get_language_version(const std::string& language);
  bool is_language_project(const std::string& language);
};