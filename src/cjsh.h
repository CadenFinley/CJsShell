#pragma once

#include <cstdint>
#include <memory>
#include <string>

class Shell;
extern std::unique_ptr<Shell> g_shell;
extern bool g_exit_flag;
extern bool g_startup_active;
extern std::uint64_t g_command_sequence;
