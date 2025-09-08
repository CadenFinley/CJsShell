#include "trap_command.h"
#include <algorithm>
#include <csignal>
#include <iostream>
#include <sstream>
#include "shell.h"
#include "cjsh.h"

// Signal name to number mapping
static const std::unordered_map<std::string, int> signal_map = {
    {"HUP", SIGHUP},    {"INT", SIGINT},    {"QUIT", SIGQUIT},
    {"ILL", SIGILL},    {"TRAP", SIGTRAP},  {"ABRT", SIGABRT},
    {"BUS", SIGBUS},    {"FPE", SIGFPE},    {"KILL", SIGKILL},
    {"USR1", SIGUSR1},  {"SEGV", SIGSEGV},  {"USR2", SIGUSR2},
    {"PIPE", SIGPIPE},  {"ALRM", SIGALRM},  {"TERM", SIGTERM},
    {"CHLD", SIGCHLD},  {"CONT", SIGCONT},  {"STOP", SIGSTOP},
    {"TSTP", SIGTSTP},  {"TTIN", SIGTTIN},  {"TTOU", SIGTTOU},
    {"URG", SIGURG},    {"XCPU", SIGXCPU},  {"XFSZ", SIGXFSZ},
    {"VTALRM", SIGVTALRM}, {"PROF", SIGPROF}, {"WINCH", SIGWINCH},
    {"IO", SIGIO},      {"SYS", SIGSYS}
};

// Reverse mapping for signal number to name
static std::unordered_map<int, std::string> reverse_signal_map;

// Initialize reverse mapping
static void init_reverse_signal_map() {
  if (reverse_signal_map.empty()) {
    for (const auto& pair : signal_map) {
      reverse_signal_map[pair.second] = pair.first;
    }
  }
}

TrapManager& TrapManager::instance() {
  static TrapManager instance;
  return instance;
}

void TrapManager::set_trap(int signal, const std::string& command) {
  if (signal == SIGKILL || signal == SIGSTOP) {
    // Cannot trap SIGKILL or SIGSTOP
    return;
  }
  
  traps[signal] = command;
  
  // Set up signal handler
  struct sigaction sa;
  sa.sa_handler = [](int sig) {
    TrapManager::instance().execute_trap(sig);
  };
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(signal, &sa, nullptr);
}

void TrapManager::remove_trap(int signal) {
  traps.erase(signal);
  
  // Reset to default signal handler
  struct sigaction sa;
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(signal, &sa, nullptr);
}

std::string TrapManager::get_trap(int signal) const {
  auto it = traps.find(signal);
  return it != traps.end() ? it->second : "";
}

void TrapManager::execute_trap(int signal) {
  auto it = traps.find(signal);
  if (it != traps.end() && shell_ref) {
    if (g_debug_mode) {
      std::cerr << "DEBUG: Executing trap for signal " << signal 
                << ": " << it->second << std::endl;
    }
    shell_ref->execute(it->second);
  }
}

std::vector<std::pair<int, std::string>> TrapManager::list_traps() const {
  std::vector<std::pair<int, std::string>> result;
  for (const auto& pair : traps) {
    result.push_back(pair);
  }
  return result;
}

void TrapManager::reset_all_traps() {
  for (const auto& pair : traps) {
    remove_trap(pair.first);
  }
  traps.clear();
}

bool TrapManager::has_trap(int signal) const {
  return traps.find(signal) != traps.end();
}

void TrapManager::set_shell(Shell* shell) {
  shell_ref = shell;
}

int signal_name_to_number(const std::string& signal_name) {
  std::string upper_name = signal_name;
  std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
  
  // Remove SIG prefix if present
  if (upper_name.substr(0, 3) == "SIG") {
    upper_name = upper_name.substr(3);
  }
  
  auto it = signal_map.find(upper_name);
  if (it != signal_map.end()) {
    return it->second;
  }
  
  // Try to parse as number
  try {
    return std::stoi(signal_name);
  } catch (...) {
    return -1;
  }
}

std::string signal_number_to_name(int signal_number) {
  init_reverse_signal_map();
  auto it = reverse_signal_map.find(signal_number);
  return it != reverse_signal_map.end() ? it->second : std::to_string(signal_number);
}

int trap_command(const std::vector<std::string>& args) {
  if (args.size() == 1) {
    // List all traps
    auto& trap_manager = TrapManager::instance();
    auto traps = trap_manager.list_traps();
    
    if (traps.empty()) {
      return 0;
    }
    
    for (const auto& pair : traps) {
      std::cout << "trap -- '" << pair.second << "' " 
                << signal_number_to_name(pair.first) << std::endl;
    }
    return 0;
  }
  
  // Handle options
  if (args.size() >= 2 && args[1] == "-l") {
    // List all signal names
    init_reverse_signal_map();
    for (const auto& pair : signal_map) {
      std::cout << pair.second << ") SIG" << pair.first << std::endl;
    }
    return 0;
  }
  
  if (args.size() >= 2 && args[1] == "-p") {
    // Print traps in a format suitable for input
    auto& trap_manager = TrapManager::instance();
    auto traps = trap_manager.list_traps();
    
    for (const auto& pair : traps) {
      std::cout << "trap -- '" << pair.second << "' " 
                << signal_number_to_name(pair.first) << std::endl;
    }
    return 0;
  }
  
  if (args.size() < 3) {
    std::cerr << "trap: usage: trap [-lp] [arg] [signal ...]" << std::endl;
    return 2;
  }
  
  // Set or remove trap
  std::string command = args[1];
  auto& trap_manager = TrapManager::instance();
  
  // Process all signal arguments
  for (size_t i = 2; i < args.size(); ++i) {
    int signal_num = signal_name_to_number(args[i]);
    if (signal_num == -1) {
      std::cerr << "trap: " << args[i] << ": invalid signal specification" << std::endl;
      return 1;
    }
    
    if (command.empty() || command == "-") {
      // Remove trap
      trap_manager.remove_trap(signal_num);
    } else {
      // Set trap
      trap_manager.set_trap(signal_num, command);
    }
  }
  
  return 0;
}
