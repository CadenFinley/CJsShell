#include <cstdint>
#include <memory>

#include "cjsh.h"
#include "shell.h"

std::unique_ptr<Shell> g_shell;
bool g_exit_flag = false;
bool g_startup_active = false;
bool g_force_exit_requested = false;
std::uint64_t g_command_sequence = 0;
