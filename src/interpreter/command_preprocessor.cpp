#include "command_preprocessor.h"

#include <limits>

#include "parser/quote_info.h"

CommandPreprocessor::PreprocessedCommand CommandPreprocessor::preprocess(
    const std::string& command) {
    PreprocessedCommand result;
    result.processed_text = command;

    result.processed_text = process_here_documents(result.processed_text, result.here_documents);

    std::string original_text = result.processed_text;
    result.processed_text = process_subshells(result.processed_text);
    result.has_subshells = (original_text != result.processed_text);

    result.needs_special_handling = !result.here_documents.empty() || result.has_subshells;
    return result;
}

std::string CommandPreprocessor::process_here_documents(
    const std::string& command, std::map<std::string, std::string>& here_docs) {
    std::string result = command;

    size_t here_pos = result.find("<<");
    if (here_pos == std::string::npos) {
        return result;
    }

    size_t delim_start = here_pos + 2;
    while (delim_start < result.size() && (std::isspace(result[delim_start]) != 0)) {
        delim_start++;
    }

    size_t delim_end = delim_start;
    while (delim_end < result.size() && (std::isspace(result[delim_end]) == 0)) {
        delim_end++;
    }

    if (delim_start >= delim_end) {
        return result;
    }

    std::string delimiter = result.substr(delim_start, delim_end - delim_start);

    bool delimiter_quoted = false;
    if (delimiter.length() >= 2) {
        if ((delimiter.front() == '\'' && delimiter.back() == '\'') ||
            (delimiter.front() == '"' && delimiter.back() == '"')) {
            delimiter_quoted = true;

            delimiter = delimiter.substr(1, delimiter.length() - 2);
        }
    }

    size_t content_start = result.find('\n', delim_end);
    if (content_start == std::string::npos) {
        return result;
    }
    content_start++;

    std::string delimiter_pattern = "\n" + delimiter;
    size_t content_end = result.find(delimiter_pattern, content_start);

    if (content_end == std::string::npos) {
        return result;
    }

    std::string content = result.substr(content_start, content_end - content_start);

    std::string placeholder = "HEREDOC_PLACEHOLDER_" + std::to_string(next_placeholder_id());

    std::string rest_of_line = result.substr(delim_end, content_start - delim_end);

    std::string stored_content = content;
    if (!delimiter_quoted) {
        stored_content = "__EXPAND__" + content;
    }
    here_docs[placeholder] = stored_content;

    std::string before_here = result.substr(0, here_pos);
    std::string after_delimiter = result.substr(content_end + delimiter.length() + 1);

    result = before_here + "< " + placeholder + rest_of_line + after_delimiter;

    return result;
}

std::string CommandPreprocessor::process_subshells(const std::string& command) {
    std::string result = command;

    if (result.empty()) {
        return result;
    }
    size_t lead = result.find_first_not_of(" \t\r\n");
    if (lead == std::string::npos || (result[lead] != '(' && result[lead] != '{')) {
        return result;
    }

    size_t close_pos = std::string::npos;

    const bool is_paren_group = result[lead] == '(';

    if (is_paren_group) {
        close_pos = find_matching_paren(result, lead);
    } else if (result[lead] == '{') {
        close_pos = find_matching_brace(result, lead);
    }

    if (close_pos == std::string::npos) {
        return result;
    }

    std::string subshell_content = result.substr(lead + 1, close_pos - (lead + 1));
    std::string remaining = result.substr(close_pos + 1);

    if (!is_paren_group) {
        size_t start = subshell_content.find_first_not_of(" \t\n\r");
        size_t end = subshell_content.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            subshell_content = subshell_content.substr(start, end - start + 1);
        }

        if (!subshell_content.empty() && subshell_content.back() == ';') {
            subshell_content.pop_back();

            end = subshell_content.find_last_not_of(" \t\n\r");
            if (end != std::string::npos) {
                subshell_content = subshell_content.substr(0, end + 1);
            }
        }
    }

    std::string prefix = result.substr(0, lead);
    const std::string marker = is_paren_group ? "SUBSHELL{" : "BRACEGROUP{";
    result = prefix + marker + subshell_content + "}" + remaining;

    return result;
}

std::string CommandPreprocessor::generate_placeholder() {
    return "HEREDOC_PLACEHOLDER_" + std::to_string(next_placeholder_id());
}

std::uint32_t CommandPreprocessor::next_placeholder_id() {
    static std::uint32_t counter = 0;
    if (counter == std::numeric_limits<std::uint32_t>::max()) {
        counter = 0;
    }
    return ++counter;
}

size_t CommandPreprocessor::find_matching_paren(const std::string& text, size_t start_pos) {
    if (start_pos >= text.length() || text[start_pos] != '(') {
        return std::string::npos;
    }

    int depth = 0;
    for (size_t i = start_pos; i < text.length(); ++i) {
        if (is_inside_quotes(text, i)) {
            continue;
        }

        if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

size_t CommandPreprocessor::find_matching_brace(const std::string& text, size_t start_pos) {
    if (start_pos >= text.length() || text[start_pos] != '{') {
        return std::string::npos;
    }

    int depth = 0;
    for (size_t i = start_pos; i < text.length(); ++i) {
        if (is_inside_quotes(text, i)) {
            continue;
        }

        if (text[i] == '{') {
            depth++;
        } else if (text[i] == '}') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}
