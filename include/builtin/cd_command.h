#pragma once

#include <string>
#include <unordered_map>

int change_directory(const std::string& dir, std::string& current_directory,
                     std::string& previous_directory,
                     std::string& last_terminal_output_error);

int change_directory_with_bookmarks(
    const std::string& dir, std::string& current_directory,
    std::string& previous_directory, std::string& last_terminal_output_error,
    std::unordered_map<std::string, std::string>& directory_bookmarks);
