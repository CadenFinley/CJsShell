#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class Parser;

namespace case_evaluator {

struct CaseSectionData {
    std::string raw_pattern;
    std::string pattern;
    std::string command;
};

std::pair<std::string, size_t> collect_case_body(const std::vector<std::string>& src_lines,
                                                 size_t start_index, Parser* parser);

std::vector<std::string> split_case_sections(const std::string& input, bool trim_sections);

std::string normalize_case_pattern(std::string pattern, Parser* parser);

bool parse_case_section(const std::string& section, CaseSectionData& out, Parser* parser);

bool execute_case_sections(
    const std::vector<std::string>& sections, const std::string& case_value,
    const std::function<int(const std::string&)>& executor, int& matched_exit_code, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher);

std::string sanitize_case_patterns(const std::string& patterns);

std::pair<bool, int> evaluate_case_patterns(
    const std::string& patterns, const std::string& case_value, bool trim_sections,
    const std::function<int(const std::string&)>& executor, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher);

std::optional<int> handle_inline_case(
    const std::string& text, const std::function<int(const std::string&)>& executor,
    bool allow_command_substitution, bool trim_sections, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher,
    const std::function<std::pair<std::string, std::vector<std::string>>(const std::string&)>&
        command_substitution_expander);

}  // namespace case_evaluator
