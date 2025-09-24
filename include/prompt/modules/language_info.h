#pragma once

#include <filesystem>
#include <string>
#include <vector>

class LanguageInfo {
 private:
  // File detection patterns following Starship's approach
  // Python detection patterns
  std::vector<std::string> python_files = {
      "requirements.txt", "requirements-dev.txt",
      "pyproject.toml",   "Pipfile",
      "Pipfile.lock",     "setup.py",
      "setup.cfg",        "tox.ini",
      ".python-version",  "environment.yml",
      "conda.yml",        "__init__.py"};
  std::vector<std::string> python_extensions = {".py", ".ipynb"};
  std::vector<std::string> python_folders = {};

  // Node.js detection patterns (following Starship exactly)
  std::vector<std::string> nodejs_files = {"package.json", ".node-version",
                                           ".nvmrc"};
  std::vector<std::string> nodejs_extensions = {".js", ".mjs", ".cjs",
                                                ".ts", ".mts", ".cts"};
  std::vector<std::string> nodejs_folders = {"node_modules"};

  // Rust detection patterns
  std::vector<std::string> rust_files = {"Cargo.toml"};
  std::vector<std::string> rust_extensions = {".rs"};
  std::vector<std::string> rust_folders = {};

  // Go detection patterns (following Starship)
  std::vector<std::string> golang_files = {
      "go.mod",    "go.sum",     "go.work",    "glide.yaml",
      "Gopkg.yml", "Gopkg.lock", ".go-version"};
  std::vector<std::string> golang_extensions = {".go"};
  std::vector<std::string> golang_folders = {"Godeps"};

  // Java detection patterns (following Starship)
  std::vector<std::string> java_files = {
      "pom.xml",  "build.gradle.kts", "build.sbt",  ".java-version",
      "deps.edn", "project.clj",      "build.boot", ".sdkmanrc"};
  std::vector<std::string> java_extensions = {".java", ".class", ".gradle",
                                              ".jar",  ".cljs",  ".cljc"};
  std::vector<std::string> java_folders = {};

  bool is_project_detected(const std::vector<std::string>& files,
                           const std::vector<std::string>& extensions,
                           const std::vector<std::string>& folders);
  bool scan_directory_recursive(const std::filesystem::path& dir,
                                const std::vector<std::string>& files,
                                const std::vector<std::string>& extensions,
                                const std::vector<std::string>& folders,
                                int max_depth = 3);
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