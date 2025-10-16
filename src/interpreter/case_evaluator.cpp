#include "case_evaluator.h"

#include <sstream>

#include "parser.h"
#include "parser_utils.h"
#include "shell_script_interpreter_utils.h"

using shell_script_interpreter::detail::strip_inline_comment;
using shell_script_interpreter::detail::trim;

namespace case_evaluator {

std::pair<std::string, size_t> collect_case_body(const std::vector<std::string>& src_lines,
                                                 size_t start_index, Parser* parser) {
    (void)parser;  // Currently unused, but kept for future use
    std::ostringstream body_stream;
    bool appended = false;
    size_t end_index = start_index;
    for (size_t i = start_index; i < src_lines.size(); ++i) {
        std::string raw = strip_inline_comment(src_lines[i]);
        std::string trimmed_line = trim(raw);
        if (trimmed_line.empty())
            continue;
        size_t esac_pos = trimmed_line.find("esac");
        if (esac_pos != std::string::npos) {
            std::string before_esac = trim(trimmed_line.substr(0, esac_pos));
            if (!before_esac.empty()) {
                if (appended)
                    body_stream << '\n';
                body_stream << before_esac;
                appended = true;
            }
            end_index = i;
            return {body_stream.str(), end_index};
        }
        if (appended)
            body_stream << '\n';
        body_stream << trimmed_line;
        appended = true;
    }
    return {body_stream.str(), src_lines.size()};
}

std::vector<std::string> split_case_sections(const std::string& input, bool trim_sections) {
    std::vector<std::string> sections;
    sections.reserve(4);
    size_t start = 0;
    while (start < input.length()) {
        size_t sep_pos = input.find(";;", start);
        std::string section;
        if (sep_pos == std::string::npos) {
            section.assign(input, start, std::string::npos);
            start = input.length();
        } else {
            section.assign(input, start, sep_pos - start);
            start = sep_pos + 2;
        }
        if (trim_sections)
            section = trim(section);
        sections.push_back(std::move(section));
    }
    return sections;
}

std::string normalize_case_pattern(std::string pattern, Parser* parser) {
    if (pattern.length() >= 2) {
        char first_char = pattern.front();
        char last_char = pattern.back();
        if ((first_char == '"' && last_char == '"') || (first_char == '\'' && last_char == '\'')) {
            pattern = pattern.substr(1, pattern.length() - 2);
        }
    }
    std::string processed;
    processed.reserve(pattern.length());
    for (size_t i = 0; i < pattern.length(); ++i) {
        if (pattern[i] == '\\' && i + 1 < pattern.length()) {
            processed += pattern[i + 1];
            ++i;
        } else {
            processed += pattern[i];
        }
    }
    if (parser != nullptr)
        parser->expand_env_vars(processed);
    return processed;
}

bool parse_case_section(const std::string& section, CaseSectionData& out, Parser* parser) {
    size_t paren_pos = section.find(')');
    if (paren_pos == std::string::npos)
        return false;
    out.raw_pattern = trim(section.substr(0, paren_pos));
    out.command = trim(section.substr(paren_pos + 1));
    if (out.command.length() >= 2 && out.command.substr(out.command.length() - 2) == ";;") {
        out.command = trim(out.command.substr(0, out.command.length() - 2));
    }
    out.pattern = normalize_case_pattern(out.raw_pattern, parser);
    return true;
}

bool execute_case_sections(
    const std::vector<std::string>& sections, const std::string& case_value,
    const std::function<int(const std::string&)>& executor, int& matched_exit_code, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher) {
    matched_exit_code = 0;
    std::vector<std::string> filtered_sections;
    filtered_sections.reserve(sections.size());
    for (const auto& raw_section : sections) {
        std::string trimmed_section = trim(raw_section);
        if (!trimmed_section.empty())
            filtered_sections.push_back(trimmed_section);
    }

    for (const auto& section : filtered_sections) {
        CaseSectionData data;
        if (!parse_case_section(section, data, parser))
            continue;

        bool pattern_matches = pattern_matcher(case_value, data.pattern);
        if (!pattern_matches)
            continue;

        if (!data.command.empty()) {
            if (parser != nullptr) {
                auto semicolon_commands = parser->parse_semicolon_commands(data.command, true);
                for (const auto& subcmd : semicolon_commands) {
                    matched_exit_code = executor(subcmd);
                    if (matched_exit_code != 0)
                        break;
                }
            } else {
                matched_exit_code = executor(data.command);
            }
        }

        return true;
    }

    return false;
}

std::string sanitize_case_patterns(const std::string& patterns) {
    size_t esac_pos = patterns.rfind("esac");
    if (esac_pos != std::string::npos)
        return patterns.substr(0, esac_pos);
    return patterns;
}

std::pair<bool, int> evaluate_case_patterns(
    const std::string& patterns, const std::string& case_value, bool trim_sections,
    const std::function<int(const std::string&)>& executor, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher) {
    auto sanitized = sanitize_case_patterns(patterns);
    auto sections = split_case_sections(sanitized, trim_sections);
    int matched_exit_code = 0;
    bool matched = execute_case_sections(sections, case_value, executor, matched_exit_code, parser,
                                         pattern_matcher);
    return {matched, matched_exit_code};
}

std::optional<int> handle_inline_case(
    const std::string& text, const std::function<int(const std::string&)>& executor,
    bool allow_command_substitution, bool trim_sections, Parser* parser,
    const std::function<bool(const std::string&, const std::string&)>& pattern_matcher,
    const std::function<std::pair<std::string, std::vector<std::string>>(const std::string&)>&
        command_substitution_expander) {
    if (text != "case" && text.rfind("case ", 0) != 0)
        return std::nullopt;
    if (text.find(" in ") == std::string::npos || text.find("esac") == std::string::npos)
        return std::nullopt;

    size_t in_pos = text.find(" in ");
    std::string case_part = text.substr(0, in_pos);
    std::string patterns_part = text.substr(in_pos + 4);
    std::string processed_case_part = case_part;

    if (allow_command_substitution && processed_case_part.find("$(") != std::string::npos) {
        auto expansion = command_substitution_expander(processed_case_part);
        processed_case_part = expansion.first;
    }

    std::string case_value;
    std::string raw_case_value;

    auto extract_case_value = [&]() {
        size_t space_pos = processed_case_part.find(' ');
        if (space_pos != std::string::npos && processed_case_part.substr(0, space_pos) == "case") {
            return trim(processed_case_part.substr(space_pos + 1));
        }
        return std::string{};
    };

    raw_case_value = extract_case_value();

    if (raw_case_value.empty() && parser != nullptr) {
        std::vector<std::string> case_tokens = parser->parse_command(processed_case_part);
        if (case_tokens.size() >= 2 && case_tokens[0] == "case") {
            raw_case_value = case_tokens[1];
        }
    }

    case_value = raw_case_value;

    if (case_value.length() >= 2) {
        if ((case_value.front() == '"' && case_value.back() == '"') ||
            (case_value.front() == '\'' && case_value.back() == '\'')) {
            case_value = case_value.substr(1, case_value.length() - 2);
        }
    }

    // Strip substitution literal markers from the case value
    strip_subst_literal_markers(case_value);

    if (!case_value.empty() && parser != nullptr)
        parser->expand_env_vars(case_value);

    auto case_result = evaluate_case_patterns(patterns_part, case_value, trim_sections, executor,
                                              parser, pattern_matcher);
    return case_result.first ? std::optional<int>{case_result.second} : std::optional<int>{0};
}

}  // namespace case_evaluator
