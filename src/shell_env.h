#pragma once

#include <string>
#include <utility>
#include <vector>

struct passwd;

namespace config {
extern bool login_mode;
extern bool interactive_mode;
extern bool force_interactive;
extern bool execute_command;
extern std::string cmd_to_execute;
extern bool colors_enabled;
extern bool source_enabled;
extern bool completions_enabled;
extern bool completion_learning_enabled;
extern bool syntax_highlighting_enabled;
extern bool show_version;
extern bool show_help;
extern bool startup_test;
extern bool minimal_mode;
extern bool show_startup_time;
extern bool secure_mode;
extern bool show_title_line;
extern bool history_expansion_enabled;
extern bool newline_after_execution;
extern bool suppress_sh_warning;
}  // namespace config

namespace cjsh_env {

void setup_environment_variables(const char* argv0 = nullptr);
void setup_path_variables(const struct passwd* pw);
std::vector<std::pair<std::string, std::string>> setup_user_system_vars(const struct passwd* pw);

bool update_terminal_dimensions();

bool is_valid_env_name(const std::string& name);
size_t collect_env_assignments(const std::vector<std::string>& args,
                               std::vector<std::pair<std::string, std::string>>& env_assignments);
void apply_env_assignments(const std::vector<std::pair<std::string, std::string>>& env_assignments);
std::vector<std::string> parse_shell_command(const std::string& command);
std::vector<char*> build_exec_argv(const std::vector<std::string>& args);

}  // namespace cjsh_env
