#include "cjsh_filesystem.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace cjsh_filesystem {

bool should_refresh_executable_cache() {
  namespace fs = cjsh_filesystem::fs;
  try {
    if (!fs::exists(g_cjsh_found_executables_path)) return true;
    auto last = fs::last_write_time(g_cjsh_found_executables_path);
    auto now = decltype(last)::clock::now();
    return (now - last) > std::chrono::hours(24);
  } catch (...) {
    return true;
  }
}

bool build_executable_cache() {
  const char* path_env = std::getenv("PATH");
  if (!path_env) return false;
  std::stringstream ss(path_env);
  std::string dir;
  std::vector<fs::path> executables;
  while (std::getline(ss, dir, ':')) {
    fs::path p(dir);
    if (!fs::is_directory(p)) continue;
    try {
      for (auto& entry : fs::directory_iterator(
               p, fs::directory_options::skip_permission_denied)) {
        auto perms = fs::status(entry.path()).permissions();
        if (fs::is_regular_file(entry.path()) &&
            (perms & fs::perms::owner_exec) != fs::perms::none) {
          executables.push_back(entry.path());
        }
      }
    } catch (const fs::filesystem_error& e) {
    }
  }
  std::ofstream ofs(g_cjsh_found_executables_path);
  if (!ofs.is_open()) return false;
  for (auto& e : executables) ofs << e.filename().string() << "\n";
  return true;
}

std::vector<fs::path> read_cached_executables() {
  std::vector<fs::path> executables;
  std::ifstream ifs(g_cjsh_found_executables_path);
  if (!ifs.is_open()) return executables;
  std::string line;
  while (std::getline(ifs, line)) {
    executables.emplace_back(line);
  }
  return executables;
}

}  // namespace cjsh_filesystem

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __linux__
#include <string.h>
extern char* program_invocation_name;
#endif

cjsh_filesystem::fs::path cjsh_filesystem::g_cjsh_path;

bool initialize_cjsh_path() {
  char path[PATH_MAX];
#ifdef __linux__
  if (readlink("/proc/self/exe", path, PATH_MAX) != -1) {
    cjsh_filesystem::g_cjsh_path = path;
    return true;
  }
#endif

#ifdef __APPLE__
  uint32_t size = PATH_MAX;
  if (_NSGetExecutablePath(path, &size) == 0) {
    char* resolved_path = realpath(path, NULL);
    if (resolved_path != nullptr) {
      cjsh_filesystem::g_cjsh_path = resolved_path;
      free(resolved_path);
      return true;
    } else {
      cjsh_filesystem::g_cjsh_path = path;
      return true;
    }
  }
#endif

  if (cjsh_filesystem::g_cjsh_path.empty()) {
#ifdef __linux__
    char* exePath = realpath(program_invocation_name, NULL);
    if (exePath) {
      cjsh_filesystem::g_cjsh_path = exePath;
      free(exePath);
      return true;
    } else {
      cjsh_filesystem::g_cjsh_path = "/usr/local/bin/cjsh";
      return true;
    }
#else
    cjsh_filesystem::g_cjsh_path = "/usr/local/bin/cjsh";
    return true;
#endif
  }
  cjsh_filesystem::g_cjsh_path = "No path found";
  return false;
}

bool initialize_cjsh_directories() {
  try {
    namespace fs = cjsh_filesystem::fs;

    // .config directory exists
    if (!fs::exists(cjsh_filesystem::g_config_path)) {
      fs::create_directories(cjsh_filesystem::g_config_path);
    }

    // .cache directory exists
    if (!fs::exists(cjsh_filesystem::g_cache_path)) {
      fs::create_directories(cjsh_filesystem::g_cache_path);
    }

    // cjsh data directory exists
    if (!fs::exists(cjsh_filesystem::g_cjsh_data_path)) {
      fs::create_directories(cjsh_filesystem::g_cjsh_data_path);
    }

    // cjsh cache directory exists
    if (!fs::exists(cjsh_filesystem::g_cjsh_cache_path)) {
      fs::create_directories(cjsh_filesystem::g_cjsh_cache_path);
    }

    if (!fs::exists(cjsh_filesystem::g_cjsh_plugin_path)) {
      fs::create_directories(cjsh_filesystem::g_cjsh_plugin_path);
    }
    if (!fs::exists(cjsh_filesystem::g_cjsh_theme_path)) {
      fs::create_directories(cjsh_filesystem::g_cjsh_theme_path);
    }
    if (!fs::exists(cjsh_filesystem::g_cjsh_colors_path)) {
      fs::create_directories(cjsh_filesystem::g_cjsh_colors_path);
    }
    return true;
  } catch (const cjsh_filesystem::fs::filesystem_error& e) {
    std::cerr << "Error creating cjsh directories: " << e.what() << std::endl;
    return false;
  }
}
