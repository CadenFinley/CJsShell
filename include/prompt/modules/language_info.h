#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class LanguageInfo {
 private:
  // Caching infrastructure
  struct CachedVersion {
    std::string version;
    std::chrono::steady_clock::time_point timestamp;
    bool is_valid() const {
      auto now = std::chrono::steady_clock::now();
      auto age =
          std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);
      return age.count() < 300;  // Cache for 5 minutes
    }
  };

  mutable std::unordered_map<std::string, CachedVersion> version_cache;
  mutable std::mutex cache_mutex;

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
      "pom.xml",     "build.gradle.kts", ".java-version", "deps.edn",
      "project.clj", "build.boot",       ".sdkmanrc"};
  std::vector<std::string> java_extensions = {".java", ".class", ".gradle",
                                              ".jar",  ".cljs",  ".cljc"};
  std::vector<std::string> java_folders = {};

  // C/C++ detection patterns
  std::vector<std::string> cpp_files = {
      "CMakeLists.txt", "Makefile",    "makefile",   "configure.ac",
      "configure.in",   "meson.build", "SConstruct", "vcpkg.json",
      "conanfile.txt",  "conanfile.py"};
  std::vector<std::string> cpp_extensions = {
      ".c", ".cpp", ".cxx", ".cc", ".c++", ".h", ".hpp", ".hxx", ".hh", ".h++"};
  std::vector<std::string> cpp_folders = {"build", "cmake"};

  // C# detection patterns
  std::vector<std::string> csharp_files = {"global.json",
                                           "project.json",
                                           "Directory.Build.props",
                                           "Directory.Build.targets",
                                           "Packages.props",
                                           ".csproj",
                                           ".sln",
                                           "nuget.config"};
  std::vector<std::string> csharp_extensions = {".cs", ".csx", ".vb"};
  std::vector<std::string> csharp_folders = {"bin", "obj"};

  // PHP detection patterns
  std::vector<std::string> php_files = {"composer.json", "composer.lock",
                                        ".php-version", "artisan"};
  std::vector<std::string> php_extensions = {".php",  ".phtml", ".php3",
                                             ".php4", ".php5",  ".phps"};
  std::vector<std::string> php_folders = {};

  // Ruby detection patterns
  std::vector<std::string> ruby_files = {
      "Gemfile", "Gemfile.lock",   ".ruby-version", "Rakefile",
      ".rvmrc",  ".rbenv-version", "config.ru",     ".irbrc"};
  std::vector<std::string> ruby_extensions = {".rb", ".rbx", ".rbi", ".gemspec",
                                              ".rake"};
  std::vector<std::string> ruby_folders = {".bundle"};

  // Kotlin detection patterns
  std::vector<std::string> kotlin_files = {"build.gradle.kts",
                                           "settings.gradle.kts"};
  std::vector<std::string> kotlin_extensions = {".kt", ".kts"};
  std::vector<std::string> kotlin_folders = {};

  // Swift detection patterns
  std::vector<std::string> swift_files = {"Package.swift", "Project.swift"};
  std::vector<std::string> swift_extensions = {".swift"};
  std::vector<std::string> swift_folders = {".swiftpm", "xcodeproj",
                                            "xcworkspace"};

  // Dart detection patterns
  std::vector<std::string> dart_files = {"pubspec.yaml", "pubspec.yml",
                                         "pubspec.lock", ".dart_tool"};
  std::vector<std::string> dart_extensions = {".dart"};
  std::vector<std::string> dart_folders = {"lib", ".dart_tool"};

  // Scala detection patterns
  std::vector<std::string> scala_files = {"build.sbt", "build.sc", ".scalaenv",
                                          ".sbtrc", ".sbtopts"};
  std::vector<std::string> scala_extensions = {".scala", ".sc"};
  std::vector<std::string> scala_folders = {"project"};

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

  // Cached version retrieval
  std::string get_cached_version(
      const std::string& language_key,
      const std::function<std::string()>& version_func) const;

 public:
  LanguageInfo();

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

  // Cache management
  void clear_version_cache();
};