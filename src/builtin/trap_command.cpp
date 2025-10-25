#include "trap_command.h"

#include "builtin_help.h"

#include <algorithm>
#include <csignal>
#include <iostream>
#include "cjsh.h"
#include "error_out.h"
#include "shell.h"
#include "signal_handler.h"

namespace {

const std::unordered_map<std::string, int> signal_map = {
    {"HUP", SIGHUP},       {"INT", SIGINT},   {"QUIT", SIGQUIT},   {"ILL", SIGILL},
    {"TRAP", SIGTRAP},     {"ABRT", SIGABRT}, {"BUS", SIGBUS},     {"FPE", SIGFPE},
    {"KILL", SIGKILL},     {"USR1", SIGUSR1}, {"SEGV", SIGSEGV},   {"USR2", SIGUSR2},
    {"PIPE", SIGPIPE},     {"ALRM", SIGALRM}, {"TERM", SIGTERM},   {"CHLD", SIGCHLD},
    {"CONT", SIGCONT},     {"STOP", SIGSTOP}, {"TSTP", SIGTSTP},   {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU},     {"URG", SIGURG},   {"XCPU", SIGXCPU},   {"XFSZ", SIGXFSZ},
    {"VTALRM", SIGVTALRM}, {"PROF", SIGPROF}, {"WINCH", SIGWINCH}, {"IO", SIGIO},
    {"SYS", SIGSYS},

    {"EXIT", 0},           {"ERR", -2},       {"DEBUG", -3},       {"RETURN", -4}};

std::unordered_map<int, std::string> reverse_signal_map;

struct TrapManagerState {
    std::unordered_map<int, std::string> traps;
    Shell* shell_ref = nullptr;
    bool exit_trap_executed = false;
};

TrapManagerState& trap_manager_state() {
    static TrapManagerState state;
    return state;
}

void init_reverse_signal_map() {
    if (reverse_signal_map.empty()) {
        for (const auto& pair : signal_map) {
            reverse_signal_map[pair.second] = pair.first;
        }
    }
}

}  // namespace

void trap_manager_set_trap(int signal, const std::string& command) {
    if (signal == SIGKILL || signal == SIGSTOP) {
        return;
    }

    auto& state = trap_manager_state();

    state.traps[signal] = command;

    if (signal == 0 || signal == -2 || signal == -3 || signal == -4) {
        return;
    }

    if (command.empty() || command == "-") {
        SignalHandler::ignore_signal(signal);
    } else {
        SignalHandler::set_signal_disposition(signal, SignalDisposition::TRAPPED);
    }
}

void trap_manager_remove_trap(int signal) {
    auto& state = trap_manager_state();
    state.traps.erase(signal);
}

std::string trap_manager_get_trap(int signal) {
    auto& state = trap_manager_state();
    auto it = state.traps.find(signal);
    return it != state.traps.end() ? it->second : "";
}

void trap_manager_execute_trap(int signal) {
    auto& state = trap_manager_state();
    auto it = state.traps.find(signal);
    if (it != state.traps.end() && (state.shell_ref != nullptr)) {
        state.shell_ref->execute(it->second);
    }
}

std::vector<std::pair<int, std::string>> trap_manager_list_traps() {
    auto& state = trap_manager_state();
    std::vector<std::pair<int, std::string>> result;
    result.reserve(state.traps.size());
    for (const auto& pair : state.traps) {
        result.push_back(pair);
    }
    return result;
}

void trap_manager_reset_all_traps() {
    auto keys = trap_manager_list_traps();
    for (const auto& pair : keys) {
        trap_manager_remove_trap(pair.first);
    }
    trap_manager_state().traps.clear();
}

bool trap_manager_has_trap(int signal) {
    auto& state = trap_manager_state();
    return state.traps.find(signal) != state.traps.end();
}

void trap_manager_set_shell(Shell* shell) {
    trap_manager_state().shell_ref = shell;
}

void trap_manager_execute_exit_trap() {
    auto& state = trap_manager_state();
    if (state.exit_trap_executed) {
        return;
    }
    state.exit_trap_executed = true;

    auto it = state.traps.find(0);
    if (it != state.traps.end() && (state.shell_ref != nullptr)) {
        state.shell_ref->execute(it->second);
    }
}

void trap_manager_execute_err_trap() {
    auto& state = trap_manager_state();
    auto it = state.traps.find(-2);
    if (it != state.traps.end() && (state.shell_ref != nullptr)) {
        state.shell_ref->execute(it->second);
    }
}

void trap_manager_execute_debug_trap() {
    auto& state = trap_manager_state();
    auto it = state.traps.find(-3);
    if (it != state.traps.end() && (state.shell_ref != nullptr)) {
        state.shell_ref->execute(it->second);
    }
}

void trap_manager_execute_return_trap() {
    auto& state = trap_manager_state();
    auto it = state.traps.find(-4);
    if (it != state.traps.end() && (state.shell_ref != nullptr)) {
        state.shell_ref->execute(it->second);
    }
}

int signal_name_to_number(const std::string& signal_name) {
    std::string upper_name = signal_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);

    if (upper_name.substr(0, 3) == "SIG") {
        upper_name = upper_name.substr(3);
    }

    if (upper_name == "EXIT" || signal_name == "0") {
        return 0;
    }
    if (upper_name == "ERR") {
        return -2;
    }
    if (upper_name == "DEBUG") {
        return -3;
    }
    if (upper_name == "RETURN") {
        return -4;
    }

    auto it = signal_map.find(upper_name);
    if (it != signal_map.end()) {
        return it->second;
    }

    try {
        int num = std::stoi(signal_name);

        if (num == 0) {
            return 0;
        }
        return num;
    } catch (...) {
        return -1;
    }
}

std::string signal_number_to_name(int signal_number) {
    switch (signal_number) {
        case 0:
            return "EXIT";
        case -2:
            return "ERR";
        case -3:
            return "DEBUG";
        case -4:
            return "RETURN";
        default:
            break;
    }

    init_reverse_signal_map();
    auto it = reverse_signal_map.find(signal_number);
    return it != reverse_signal_map.end() ? it->second : std::to_string(signal_number);
}

int trap_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: trap [-lp] [ARG] [SIGNAL ...]",
                                   "Set a command to execute when SIGNAL is received.",
                                   "With no arguments, list active traps."})) {
        return 0;
    }
    if (args.size() == 1) {
        auto traps = trap_manager_list_traps();

        if (traps.empty()) {
            return 0;
        }

        for (const auto& pair : traps) {
            std::cout << "trap -- '" << pair.second << "' " << signal_number_to_name(pair.first)
                      << '\n';
        }
        return 0;
    }

    if (args.size() >= 2 && args[1] == "-l") {
        init_reverse_signal_map();
        for (const auto& pair : signal_map) {
            std::cout << pair.second << ") SIG" << pair.first << '\n';
        }
        return 0;
    }

    if (args.size() >= 2 && args[1] == "-p") {
        auto traps = trap_manager_list_traps();

        for (const auto& pair : traps) {
            std::cout << "trap -- '" << pair.second << "' " << signal_number_to_name(pair.first)
                      << '\n';
        }
        return 0;
    }

    if (args.size() < 3) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "trap", "usage: trap [-lp] [arg] [signal ...]", {}});
        return 2;
    }

    const std::string& command = args[1];
    for (size_t i = 2; i < args.size(); ++i) {
        int signal_num = signal_name_to_number(args[i]);
        if (signal_num == -1) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "trap",
                         args[i] + ": invalid signal specification",
                         {}});
            return 1;
        }

        if (command.empty() || command == "-") {
            trap_manager_remove_trap(signal_num);
        } else {
            trap_manager_set_trap(signal_num, command);
        }
    }

    return 0;
}
