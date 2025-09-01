#include "alias_command.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "cjsh_filesystem.h"
#include "cjsh.h"

#define PRINT_ERROR(MSG)      \
  do {                        \
    std::cerr << MSG << '\n'; \
  } while (0)

int alias_command(const std::vector<std::string>& args, Shell* shell) {
  if (args.size() == 1) {
    auto& aliases = shell->get_aliases();
    if (aliases.empty()) {
      std::cout << "No aliases defined." << std::endl;
    } else {
      for (const auto& [name, value] : aliases) {
        std::cout << "alias " << name << "='" << value << "'" << std::endl;
      }
    }
    return 0;
  }

  bool all_successful = true;
  auto& aliases = shell->get_aliases();

  for (size_t i = 1; i < args.size(); ++i) {
    std::string name, value;
    if (parse_assignment(args[i], name, value)) {
      aliases[name] = value;
      save_alias_to_file(name, value);
      if (g_debug_mode) {
        std::cout << "Added alias: " << name << "='" << value << "'"
                  << std::endl;
      }
    } else {
      PRINT_ERROR("alias: invalid assignment: " + args[i]);
      all_successful = false;
    }
  }

  if (shell) {
    shell->set_aliases(aliases);
  }

  return all_successful ? 0 : 1;
}

int unalias_command(const std::vector<std::string>& args, Shell* shell) {
  if (args.size() < 2) {
    std::cerr << "unalias: not enough arguments" << std::endl;
    return 1;
  }

  bool success = true;
  auto& aliases = shell->get_aliases();

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& name = args[i];
    auto it = aliases.find(name);

    if (it != aliases.end()) {
      aliases.erase(it);
      remove_alias_from_file(name);
      if (g_debug_mode) {
        std::cout << "Removed alias: " << name << std::endl;
      }
    } else {
      std::cerr << "unalias: " << name << ": not found" << std::endl;
      success = false;
    }
  }

  if (shell) {
    shell->set_aliases(aliases);
  }

  return success ? 0 : 1;
}

bool parse_assignment(const std::string& arg, std::string& name,
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

int save_alias_to_file(const std::string& name, const std::string& value) {
  std::filesystem::path source_path = cjsh_filesystem::g_cjsh_source_path;

  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(source_path);

  bool alias_found = false;

  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("alias " + name + "=") == 0) {
        lines.push_back("alias " + name + "='" + value + "'");
        alias_found = true;
      } else {
        lines.push_back(line);
      }
    }
    read_file.close();
  }

  if (!alias_found) {
    lines.push_back("alias " + name + "='" + value + "'");
  }

  std::ofstream write_file(source_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();

    if (g_debug_mode) {
      std::cout << "Alias " << name << " saved to " << source_path.string()
                << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open source file for writing at " +
                source_path.string());
  }
  return 0;
}

int remove_alias_from_file(const std::string& name) {
  std::filesystem::path source_path = cjsh_filesystem::g_cjsh_source_path;

  std::vector<std::string> lines;
  std::string line;
  std::ifstream read_file(source_path);

  if (read_file.is_open()) {
    while (std::getline(read_file, line)) {
      if (line.find("alias " + name + "=") != 0) {
        lines.push_back(line);
      }
    }
    read_file.close();
  }

  std::ofstream write_file(source_path);
  if (write_file.is_open()) {
    for (const auto& l : lines) {
      write_file << l << std::endl;
    }
    write_file.close();

    if (g_debug_mode) {
      std::cout << "Alias " << name << " removed from " << source_path.string()
                << std::endl;
    }
  } else {
    PRINT_ERROR("Error: Unable to open source file for writing at " +
                source_path.string());
  }
  return 0;
}
