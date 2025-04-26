#include "built_ins.h"

bool Built_ins::builtin_command(const std::vector<std::string>& args) {
  if (args.empty()) return false;

  // Check if the command is in the builtins map
  auto it = builtins.find(args[0]);
  if (it != builtins.end()) {
    return it->second(args);
  }

  // If not found, return false
  return false;
}

bool Built_ins::change_directory(const std::string& dir, std::string& result) {
  std::string target_dir = dir;
  
  // Handle empty input or just "~"
  if (target_dir.empty()) {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      result = "cjsh: HOME environment variable is not set";
      return false;
    }
    target_dir = home_dir;
  }
  
  // Expand tilde at the beginning of the path
  if (target_dir[0] == '~') {
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
      result = "cjsh: Cannot expand '~' - HOME environment variable is not set";
      return false;
    }
    target_dir.replace(0, 1, home_dir);
  }
  
  // Create a path object from the target directory
  std::filesystem::path dir_path;
  
  try {
    // If it's an absolute path, use it directly; otherwise, make it relative to current directory
    if (std::filesystem::path(target_dir).is_absolute()) {
      dir_path = target_dir;
    } else {
      dir_path = std::filesystem::path(current_directory) / target_dir;
    }

    // Check if the directory exists
    if (!std::filesystem::exists(dir_path)) {
      result = "cd: " + target_dir + ": No such file or directory";
      return false;
    }
    
    // Check if it's a directory
    if (!std::filesystem::is_directory(dir_path)) {
      result = "cd: " + target_dir + ": Not a directory";
      return false;
    }
    
    // Get the canonical (absolute, normalized) path
    std::filesystem::path canonical_path = std::filesystem::canonical(dir_path);
    current_directory = canonical_path.string();
    
    // Actually change the working directory
    if (chdir(current_directory.c_str()) != 0) {
      result = "cd: " + std::string(strerror(errno));
      return false;
    }
    
    // Update PWD environment variable
    setenv("PWD", current_directory.c_str(), 1);
    
    return true;
  }
  catch (const std::filesystem::filesystem_error& e) {
    result = "cd: " + std::string(e.what());
    return false;
  }
  catch (const std::exception& e) {
    result = "cd: Unexpected error: " + std::string(e.what());
    return false;
  }
}

void Built_ins::ai_commands(const std::vector<std::string>& args) {
  
}

void Built_ins::do_ai_request(const std::string& prompt) {
  
}
