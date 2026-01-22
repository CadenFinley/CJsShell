#pragma once

#include <cstdint>
#include <memory>
#include <string>

class Shell;
extern std::unique_ptr<Shell> g_shell;
extern bool g_exit_flag;
extern bool g_startup_active;
extern std::uint64_t g_command_sequence;

namespace config {
extern bool login_mode;
extern bool interactive_mode;
extern bool force_interactive;
extern bool execute_command;
extern std::string cmd_to_execute;
extern bool colors_enabled;
extern bool source_enabled;
extern bool completions_enabled;
extern bool syntax_highlighting_enabled;
extern bool smart_cd_enabled;
extern bool show_version;
extern bool show_help;
extern bool startup_test;
extern bool minimal_mode;
extern bool show_startup_time;
extern bool secure_mode;
extern bool show_title_line;
extern bool history_expansion_enabled;
extern bool newline_after_execution;
extern bool uses_cleanup;
extern bool cleanup_newline_after_execution;
extern bool cleanup_adds_empty_line;
extern bool cleanup_truncates_multiline;
}  // namespace config
