#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace suggestion_utils {

std::vector<std::string> generate_command_suggestions(const std::string& command);

std::vector<std::string> generate_cd_suggestions(const std::string& target_dir,
                                                 const std::string& current_dir);

std::vector<std::string> generate_ls_suggestions(const std::string& path,
                                                 const std::string& current_dir);
int edit_distance(const std::string& str1, const std::string& str2);

std::vector<std::string> find_similar_entries(const std::string& target_name,
                                              const std::string& directory,
                                              int max_suggestions = 3);
std::vector<std::string> generate_executable_suggestions(
    const std::string& command, const std::unordered_set<std::string>& available_commands);

std::vector<std::string> generate_fuzzy_suggestions(
    const std::string& command, const std::vector<std::string>& available_commands);

int calculate_fuzzy_score(const std::string& input, const std::string& candidate);

}  // namespace suggestion_utils
