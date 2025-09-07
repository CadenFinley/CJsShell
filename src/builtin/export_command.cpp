#include "export_command.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"

#define PRINT_ERROR(MSG)      \
  do {                        \
    std::cerr << MSG << '\n'; \
  } while (0)

int export_command(const std::vector<std::string>& args, Shell* shell) {
  if (args.size() == 1) {
    extern char** environ;
    for (char** env = environ; *env; ++env) {
      std::cout << "export " << *env << std::endl;
    }
    return 0;
  }

  bool all_successful = true;
  auto& env_vars = shell->get_env_vars();

  for (size_t i = 1; i < args.size(); ++i) {
    std::string name, value;
    if (parse_env_assignment(args[i], name, value)) {
      env_vars[name] = value;

      setenv(name.c_str(), value.c_str(), 1);

      if (g_debug_mode) {
        std::cout << "Set environment variable: " << name << "='" << value
                  << "'" << std::endl;
      }
    } else {
      // Just export the existing variable (make it available to child
      // processes)
      const char* env_val = getenv(args[i].c_str());
      if (env_val) {
        // Variable exists, just mark it for export (it's already exported via
        // setenv)
        env_vars[args[i]] = env_val;
        if (g_debug_mode) {
          std::cout << "Exported existing variable: " << args[i] << "='"
                    << env_val << "'" << std::endl;
        }
      } else {
        PRINT_ERROR("export: " + args[i] + ": not found");
        all_successful = false;
      }
    }
  }

  if (shell) {
    shell->set_env_vars(env_vars);
  }

  return all_successful ? 0 : 1;
}

int unset_command(const std::vector<std::string>& args, Shell* shell) {
  if (args.size() < 2) {
    PRINT_ERROR("unset: not enough arguments");
    return 1;
  }

  bool success = true;
  auto& env_vars = shell->get_env_vars();

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& name = args[i];

    env_vars.erase(name);

    if (unsetenv(name.c_str()) != 0) {
      PRINT_ERROR(std::string("unset: error unsetting ") + name + ": " +
                  strerror(errno));
      success = false;
    } else {
      if (g_debug_mode) {
        std::cout << "Unset environment variable: " << name << std::endl;
      }
    }
  }

  if (shell) {
    shell->set_env_vars(env_vars);
  }

  return success ? 0 : 1;
}

bool parse_env_assignment(const std::string& arg, std::string& name,
                          std::string& value) {
  size_t equals_pos = arg.find('=');
  if (equals_pos == std::string::npos || equals_pos == 0) {
    return false;
  }

  name = arg.substr(0, equals_pos);
  value = arg.substr(equals_pos + 1);

  if (value.size() >= 2) {
    if ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')) {
      value = value.substr(1, value.size() - 2);
    }
  }

  return true;
}

int save_env_var_to_file(const std::string& name, const std::string& value,
                         bool login_mode) {
  if (!login_mode) {
    if (g_debug_mode) {
      std::cerr << "Warning: Attempted to save environment variable to config "
                   "file when not in login mode"
                << std::endl;
    }
    return 0;
  }

  std::filesystem::path config_path = cjsh_filesystem::g_cjsh_profile_path;

  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(config_path);

  bool env_var_found = false;

  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("export " + name + "=") == 0) {
        lines.push_back("export " + name + "='" + value + "'");
        env_var_found = true;
      } else {
        lines.push_back(line);
      }
    }
    read_file.close();
  }

  if (!env_var_found) {
    lines.push_back("export " + name + "='" + value + "'");
  }

  std::ofstream write_file(config_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();

    if (g_debug_mode) {
      std::cout << "Environment variable " << name << " saved to "
                << config_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open config file for writing at " +
                config_path.string());
  }
  return 0;
}

int remove_env_var_from_file(const std::string& name, bool login_mode) {
  if (!login_mode) {
    if (g_debug_mode) {
      std::cerr << "Warning: Attempted to remove environment variable from "
                   "config file when not in login mode"
                << std::endl;
    }
    return 0;
  }

  std::filesystem::path config_path = cjsh_filesystem::g_cjsh_profile_path;

  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(config_path);

  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("export " + name + "=") != 0) {
        lines.push_back(line);
      }
    }
    read_file.close();
  }

  std::ofstream write_file(config_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();

    if (g_debug_mode) {
      std::cout << "Environment variable " << name << " removed from "
                << config_path.string() << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open config file for writing at " +
                config_path.string());
  }
  return 0;
}
