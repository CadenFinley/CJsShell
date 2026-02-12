/*
  shell_env.h

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct passwd;
class Shell;

namespace config {
extern bool login_mode;
extern bool interactive_mode;
extern bool force_interactive;
extern bool execute_command;
extern std::string cmd_to_execute;
extern bool no_exec;
extern bool colors_enabled;
extern bool source_enabled;
extern bool completions_enabled;
extern bool completion_learning_enabled;
extern bool smart_cd_enabled;
extern bool syntax_highlighting_enabled;
extern bool show_version;
extern bool show_help;
extern bool startup_test;
extern bool minimal_mode;
extern bool show_startup_time;
extern bool secure_mode;
extern bool posix_mode;
extern bool show_title_line;
extern bool history_expansion_enabled;
extern bool newline_after_execution;
extern bool suppress_sh_warning;
extern bool status_line_enabled;
extern bool status_reporting_enabled;
extern bool script_extension_interpreter_enabled;
extern bool error_suggestions_enabled;
extern bool prompt_vars_enabled;
}  // namespace config

namespace cjsh_env {

void setup_environment_variables(const char* argv0 = nullptr);
void setup_path_variables(const struct passwd* pw);
std::vector<std::pair<std::string, std::string>> setup_user_system_vars(const struct passwd* pw);

std::string get_shell_variable_value(const std::string& name);
std::string get_shell_variable_value(const char* name);
bool shell_variable_is_set(const std::string& name);
bool shell_variable_is_set(const char* name);

bool update_terminal_dimensions();
void sync_env_vars_from_system(Shell& shell);
std::unordered_map<std::string, std::string>& env_vars();
void replace_env_vars(const std::unordered_map<std::string, std::string>& new_env_vars,
                      Shell* shell);
void sync_parser_env_vars(Shell* shell);

bool is_valid_env_name(const std::string& name);
size_t collect_env_assignments(const std::vector<std::string>& args,
                               std::vector<std::pair<std::string, std::string>>& env_assignments);
void apply_env_assignments(const std::vector<std::pair<std::string, std::string>>& env_assignments);
std::vector<std::string> parse_shell_command(const std::string& command);
std::vector<char*> build_exec_argv(const std::vector<std::string>& args);

}  // namespace cjsh_env

int handle_non_interactive_mode(const std::string& script_file);
