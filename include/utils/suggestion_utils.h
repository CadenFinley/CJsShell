#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace suggestion_utils {

/**
 * Generate suggestions for a command not found error
 * @param command The command that was not found
 * @return Vector of suggestion strings
 */
std::vector<std::string> generate_command_suggestions(const std::string& command);

/**
 * Generate suggestions for a cd directory not found error
 * @param target_dir The directory that was not found
 * @param current_dir The current working directory
 * @return Vector of suggestion strings
 */
std::vector<std::string> generate_cd_suggestions(const std::string& target_dir, 
                                                const std::string& current_dir);

/**
 * Generate suggestions for ls file/directory not found error
 * @param path The path that was not found
 * @param current_dir The current working directory  
 * @return Vector of suggestion strings
 */
std::vector<std::string> generate_ls_suggestions(const std::string& path,
                                                const std::string& current_dir);

/**
 * Calculate edit distance between two strings (for fuzzy matching)
 * @param str1 First string
 * @param str2 Second string
 * @return Edit distance
 */
int edit_distance(const std::string& str1, const std::string& str2);

/**
 * Find similar entries in a directory
 * @param target_name The name we're looking for
 * @param directory The directory to search in
 * @param max_suggestions Maximum number of suggestions to return
 * @return Vector of similar names
 */
std::vector<std::string> find_similar_entries(const std::string& target_name,
                                             const std::string& directory,
                                             int max_suggestions = 3);

} // namespace suggestion_utils