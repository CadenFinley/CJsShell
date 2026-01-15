#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Shell;
extern std::unique_ptr<Shell> g_shell;

extern const bool PRE_RELEASE;
extern const char* const c_version_base;

std::string get_version();

#ifndef CJSH_GIT_HASH
#define CJSH_GIT_HASH "unknown"
#endif

extern bool g_exit_flag;
extern bool g_startup_active;
extern std::uint64_t g_command_sequence;

std::vector<std::string>& startup_args();

std::vector<std::string>& profile_startup_args();

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

void cleanup_resources();
