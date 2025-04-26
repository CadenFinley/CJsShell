#include "built_ins.h"
#include "main.h"

bool Built_ins::builtin_command(const std::vector<std::string>& args) {
  switch (hash(args[0].c_str())) {
    case hash("exit"):
      g_exit_flag = true;
      return true;
    case hash("cd"): {
      std::string result;
      std::string dir = "";
      
      // Extract directory argument if provided
      if (args.size() > 1) {
        dir = args[1];
      }
      
      if (!change_directory(dir, result) && !result.empty()) {
        std::cerr << result << std::endl;
      }
      return true;
    }
    case hash("alias"):
      // Handle alias command
      // add to the aliases map and then add to .cjshrc
      return true;
    case hash("export"):
      // Handle export command
      // setenv and then add to the env_vars map and then add it to .cjshrc
      return true;
    case hash("unset"):
      // Handle unset command
      return true;
    case hash("source"):
      // Handle source command
      // reloads passed arg <filename>
      return true;
    case hash("unalias"):
      // Handle unalias command
      return true;
    default:
      return false;
  }
}

// static const std::unordered_map<std::string, BuiltinHandler> builtins = {
//   {"exit",   [this](auto& a){ shell->set_exit_flag(true); return true; }},
//   {"cd",     [this](auto& a){ /* … */ }},
//   …
// };
// auto it = builtins.find(args[0]);
// return it != builtins.end() ? it->second(args) : false;

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
