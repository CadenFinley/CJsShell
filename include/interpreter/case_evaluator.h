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

/**
 * Collects the body of a case statement from source lines.
 * Returns the body text and the index of the closing 'esac'.
 */
std::pair<std::string, size_t> collect_case_body(const std::vector<std::string>& src_lines,
                                                 size_t start_index, Parser* parser);

/**
 * Splits case statement body into sections delimited by ';;'.
 */
std::vector<std::string> split_case_sections(const std::string& input, bool trim_sections);

/**
 * Normalizes a case pattern by removing quotes and expanding environment variables.
 */
std::string normalize_case_pattern(std::string pattern, Parser* parser);

/**
 * Parses a single case section into pattern and command parts.
 */
bool parse_case_section(const std::string& section, CaseSectionData& out, Parser* parser);

/**
 * Executes case sections, matching patterns and running commands.
 * Returns true if a pattern matched, and sets matched_exit_code.
 */
bool execute_case_sections(
    const std::vector<std::string>& sections, const std::string& case_value,
    const std::function<int(const std::string&)>& executor, int& matched_exit_code, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher);

/**
 * Removes trailing 'esac' from case patterns if present.
 */
std::string sanitize_case_patterns(const std::string& patterns);

/**
 * Evaluates case patterns against a value and executes matching commands.
 * Returns {matched, exit_code}.
 */
std::pair<bool, int> evaluate_case_patterns(
    const std::string& patterns, const std::string& case_value, bool trim_sections,
    const std::function<int(const std::string&)>& executor, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher);

/**
 * Handles inline case statements (one-line case...in...esac).
 * Returns exit code if handled, nullopt otherwise.
 */
std::optional<int> handle_inline_case(
    const std::string& text, const std::function<int(const std::string&)>& executor,
    bool allow_command_substitution, bool trim_sections, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher,
    const std::function<std::pair<std::string, std::vector<std::string>>(const std::string&)>&
        command_substitution_expander);

}  // namespace case_evaluator
