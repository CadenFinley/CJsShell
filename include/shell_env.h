#pragma once

#include <string>
#include <utility>
#include <vector>

struct passwd;

namespace cjsh_env {

void setup_environment_variables(const char* argv0 = nullptr);
void setup_path_variables(const struct passwd* pw);
std::vector<std::pair<const char*, const char*>> setup_user_system_vars(const struct passwd* pw);

bool is_valid_env_name(const std::string& name);
size_t collect_env_assignments(const std::vector<std::string>& args,
                               std::vector<std::pair<std::string, std::string>>& env_assignments);
void apply_env_assignments(const std::vector<std::pair<std::string, std::string>>& env_assignments);
std::vector<std::string> parse_shell_command(const std::string& command);
std::vector<char*> build_exec_argv(const std::vector<std::string>& args);

}  // namespace cjsh_env
