#include "hash_command.h"
#include <iostream>
#include <unordered_map>
#include "cjsh_filesystem.h"

// Static hash table for command path caching
static std::unordered_map<std::string, std::string> command_hash;
static std::unordered_map<std::string, int> command_hits;

int hash_command(const std::vector<std::string>& args, Shell* shell) {
  (void)shell;  // Suppress unused parameter warning

  // No arguments - display all cached commands
  if (args.size() == 1) {
    if (command_hash.empty()) {
      return 0;
    }

    std::cout << "hits\tcommand" << std::endl;
    for (const auto& pair : command_hash) {
      int hits = command_hits.count(pair.first) ? command_hits[pair.first] : 0;
      std::cout << hits << "\t" << pair.second << std::endl;
    }
    return 0;
  }

  bool remove_mode = false;
  bool disable_lookup = false;
  size_t start_index = 1;

  // Parse options
  for (size_t i = 1; i < args.size() && args[i][0] == '-'; ++i) {
    const std::string& option = args[i];
    if (option == "--") {
      start_index = i + 1;
      break;
    }

    if (option == "-r") {
      remove_mode = true;
    } else if (option == "-d") {
      disable_lookup = true;
    } else if (option == "-l") {
      // List mode (default behavior with arguments)
    } else if (option == "-p") {
      // Print mode
    } else if (option == "-t") {
      // Type mode (simplified)
    } else {
      std::cerr << "hash: invalid option: " << option << std::endl;
      return 1;
    }
    start_index = i + 1;
  }

  // Handle -r with no arguments (remove all)
  if (remove_mode && start_index >= args.size()) {
    command_hash.clear();
    command_hits.clear();
    return 0;
  }

  // Process each command name
  for (size_t i = start_index; i < args.size(); ++i) {
    const std::string& name = args[i];

    if (remove_mode) {
      // Remove from hash table
      command_hash.erase(name);
      command_hits.erase(name);
    } else if (disable_lookup) {
      // Disable lookup for this command (remove from cache)
      command_hash.erase(name);
      command_hits.erase(name);
    } else {
      // Add/update in hash table
      std::string path = cjsh_filesystem::find_executable_in_path(name);
      if (!path.empty()) {
        command_hash[name] = path;
        if (command_hits.count(name) == 0) {
          command_hits[name] = 0;
        }

        // Display the result
        std::cout << path << std::endl;
      } else {
        std::cerr << "hash: " << name << ": not found" << std::endl;
        return 1;
      }
    }
  }

  return 0;
}

// Helper function to check if a command is in the hash table
std::string get_hashed_command(const std::string& name) {
  auto it = command_hash.find(name);
  if (it != command_hash.end()) {
    // Increment hit count
    command_hits[name]++;
    return it->second;
  }
  return "";
}

// Helper function to add a command to the hash table
void add_to_hash(const std::string& name, const std::string& path) {
  command_hash[name] = path;
  if (command_hits.count(name) == 0) {
    command_hits[name] = 0;
  }
}
