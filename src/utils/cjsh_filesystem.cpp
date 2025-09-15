#include "cjsh_filesystem.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace cjsh_filesystem {

fs::path g_cjsh_path;

bool should_refresh_executable_cache() {
  try {
    if (!fs::exists(g_cjsh_found_executables_path))
      return true;
    auto last = fs::last_write_time(g_cjsh_found_executables_path);
    auto now = decltype(last)::clock::now();
    return (now - last) > std::chrono::hours(24);
  } catch (...) {
    return true;
  }
}

bool build_executable_cache() {
  const char* path_env = std::getenv("PATH");
  if (!path_env)
    return false;
  std::stringstream ss(path_env);
  std::string dir;
  std::vector<fs::path> executables;
  while (std::getline(ss, dir, ':')) {
    fs::path p(dir);
    if (!fs::is_directory(p))
      continue;
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
  if (!ofs.is_open())
    return false;
  for (auto& e : executables)
    ofs << e.filename().string() << "\n";
  return true;
}

std::vector<fs::path> read_cached_executables() {
  std::vector<fs::path> executables;
  std::ifstream ifs(g_cjsh_found_executables_path);
  if (!ifs.is_open())
    return executables;
  std::string line;
  while (std::getline(ifs, line)) {
    executables.emplace_back(line);
  }
  return executables;
}

bool file_exists(const fs::path& path) {
  return fs::exists(path);
}

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

bool initialize_cjsh_path() {
  char path[PATH_MAX];
#ifdef __linux__
  if (readlink("/proc/self/exe", path, PATH_MAX) != -1) {
    g_cjsh_path = path;
    return true;
  }
#endif

#ifdef __APPLE__
  uint32_t size = PATH_MAX;
  if (_NSGetExecutablePath(path, &size) == 0) {
    char* resolved_path = realpath(path, NULL);
    if (resolved_path != nullptr) {
      g_cjsh_path = resolved_path;
      free(resolved_path);
      return true;
    } else {
      g_cjsh_path = path;
      return true;
    }
  }
#endif

  if (g_cjsh_path.empty()) {
    g_cjsh_path = "cjsh";
    return true;
  }

  return true;
}

bool initialize_cjsh_directories() {
  try {
    fs::create_directories(g_config_path);
    fs::create_directories(g_cache_path);
    fs::create_directories(g_cjsh_data_path);
    fs::create_directories(g_cjsh_cache_path);
    fs::create_directories(g_cjsh_plugin_path);
    fs::create_directories(g_cjsh_theme_path);
    fs::create_directories(g_cjsh_ai_config_path);
    fs::create_directories(g_cjsh_ai_conversations_path);

    return true;
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Error creating cjsh directories: " << e.what() << std::endl;
    return false;
  }
}

std::filesystem::path get_cjsh_path() {
  if (g_cjsh_path.empty() || g_cjsh_path == ".") {
    initialize_cjsh_path();
  }
  return g_cjsh_path;
}

std::string find_executable_in_path(const std::string& name) {
  const char* path_env = std::getenv("PATH");
  if (!path_env) {
    return "";
  }

  std::stringstream ss(path_env);
  std::string dir;

  while (std::getline(ss, dir, ':')) {
    if (dir.empty())
      continue;

    fs::path executable_path = fs::path(dir) / name;

    try {
      if (fs::exists(executable_path) && fs::is_regular_file(executable_path)) {
        auto perms = fs::status(executable_path).permissions();
        if ((perms & fs::perms::owner_exec) != fs::perms::none ||
            (perms & fs::perms::group_exec) != fs::perms::none ||
            (perms & fs::perms::others_exec) != fs::perms::none) {
          return executable_path.string();
        }
      }
    } catch (const fs::filesystem_error&) {
      continue;
    }
  }

  return "";
}

}  // namespace cjsh_filesystem
