#pragma once

#include <signal.h>
#include <string>
#include <unordered_map>
#include <vector>

class Shell;

// Signal trap management
class TrapManager {
 public:
  static TrapManager& instance();

  // Set a trap for a signal
  void set_trap(int signal, const std::string& command);

  // Remove a trap for a signal
  void remove_trap(int signal);

  // Get the command for a signal trap
  std::string get_trap(int signal) const;

  // Execute trap command for a signal
  void execute_trap(int signal);

  // List all active traps
  std::vector<std::pair<int, std::string>> list_traps() const;

  // Reset all traps
  void reset_all_traps();

  // Check if a signal has a trap
  bool has_trap(int signal) const;

  // Set shell reference for executing trap commands
  void set_shell(Shell* shell);

 private:
  TrapManager() = default;
  std::unordered_map<int, std::string> traps;
  Shell* shell_ref = nullptr;
};

// Convert signal name to signal number
int signal_name_to_number(const std::string& signal_name);

// Convert signal number to signal name
std::string signal_number_to_name(int signal_number);

// Trap builtin command
int trap_command(const std::vector<std::string>& args);
