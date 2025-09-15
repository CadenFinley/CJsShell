#pragma once

#include <signal.h>
#include <string>
#include <unordered_map>
#include <vector>

class Shell;

class TrapManager {
 public:
  static TrapManager& instance();

  void set_trap(int signal, const std::string& command);

  void remove_trap(int signal);

  std::string get_trap(int signal) const;

  void execute_trap(int signal);

  std::vector<std::pair<int, std::string>> list_traps() const;

  void reset_all_traps();

  bool has_trap(int signal) const;

  void set_shell(Shell* shell);

  void execute_exit_trap();
  void execute_err_trap();
  void execute_debug_trap();
  void execute_return_trap();

 private:
  TrapManager() = default;
  std::unordered_map<int, std::string> traps;
  Shell* shell_ref = nullptr;
  bool exit_trap_executed = false;
};

int signal_name_to_number(const std::string& signal_name);

std::string signal_number_to_name(int signal_number);

int trap_command(const std::vector<std::string>& args);
