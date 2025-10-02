#pragma once

#include <string>
#include <vector>

struct passwd;

namespace cjsh_env {

void setup_environment_variables(const char* argv0 = nullptr);
void setup_path_variables(const struct passwd* pw);
std::vector<std::pair<const char*, const char*>> setup_user_system_vars(const struct passwd* pw);

}  // namespace cjsh_env