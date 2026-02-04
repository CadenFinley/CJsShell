#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

class Shell;

namespace command_analysis {

struct CommentRange {
    size_t start = 0;
    size_t end = 0;
};

struct CommandSeparator {
    size_t length = 0;
    bool is_operator = false;
};

bool extract_next_token(const std::string& cmd, size_t& cursor, size_t& token_start,
                        size_t& token_end);

bool token_has_explicit_path_hint(const std::string& token);
std::string resolve_token_path(const std::string& token, const Shell* shell);
bool token_is_history_expansion(const std::string& token, size_t absolute_cmd_start);

bool is_known_command_token(const std::string& token, size_t absolute_cmd_start, Shell* shell,
                            const std::unordered_set<std::string>& available_commands);

std::string sanitize_input_for_analysis(const std::string& input,
                                        std::vector<CommentRange>* comment_ranges = nullptr);

CommandSeparator scan_command_separator(const std::string& analysis, size_t index);
size_t find_command_end(const std::string& analysis, size_t start);

}  // namespace command_analysis
