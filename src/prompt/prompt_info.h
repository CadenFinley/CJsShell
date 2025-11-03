#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "basic_info.h"
#include "command_info.h"
#include "container_info.h"
#include "directory_info.h"
#include "environment_info.h"
#include "git_info.h"
#include "language_info.h"
#include "network_info.h"
#include "system_info.h"
#include "time_info.h"

struct ThemeSegment;

class PromptInfo {
   public:
    std::string get_basic_prompt();
    std::string get_basic_title();
    bool is_variable_used(const std::string& var_name, const std::vector<ThemeSegment>& segments);
    std::unordered_map<std::string, std::string> get_variables(
        const std::vector<ThemeSegment>& segments, bool is_git_repo = false,
        const std::filesystem::path& repo_root = {});
    void clear_cached_state();
};
