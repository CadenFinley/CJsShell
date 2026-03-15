/*
  command_preprocessor.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "command_preprocessor.h"

#include <limits>

#include "parser_utils.h"

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

    bool strip_tabs = false;
    size_t operator_len = 2;
    if (here_pos + 2 < result.size() && result[here_pos + 2] == '-') {
        strip_tabs = true;
        operator_len = 3;
    }

    size_t delim_start = here_pos + operator_len;
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

    std::string content;
    bool first_content_line = true;
    size_t delimiter_line_start = std::string::npos;
    size_t delimiter_line_end = std::string::npos;

    size_t scan_pos = content_start;
    while (scan_pos <= result.size()) {
        size_t line_end = result.find('\n', scan_pos);
        bool has_newline = (line_end != std::string::npos);
        size_t line_len = has_newline ? line_end - scan_pos : result.size() - scan_pos;

        std::string line = result.substr(scan_pos, line_len);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string compare_line = line;
        size_t first_non_ws = compare_line.find_first_not_of(" \t");
        if (first_non_ws == std::string::npos) {
            compare_line.clear();
        } else {
            compare_line.erase(0, first_non_ws);
        }
        size_t last_non_ws = compare_line.find_last_not_of(" \t\r");
        if (last_non_ws == std::string::npos) {
            compare_line.clear();
        } else {
            compare_line.erase(last_non_ws + 1);
        }

        if (compare_line == delimiter) {
            delimiter_line_start = scan_pos;
            delimiter_line_end = has_newline ? line_end + 1 : result.size();
            break;
        }

        std::string line_to_store = line;
        if (strip_tabs) {
            size_t first_non_tab = line_to_store.find_first_not_of('\t');
            if (first_non_tab == std::string::npos) {
                line_to_store.clear();
            } else {
                line_to_store.erase(0, first_non_tab);
            }
        }

        if (!first_content_line) {
            content += '\n';
        }
        content += line_to_store;
        first_content_line = false;

        if (!has_newline) {
            scan_pos = result.size();
            break;
        }
        scan_pos = line_end + 1;
    }

    if (delimiter_line_start == std::string::npos) {
        return result;
    }

    std::string placeholder = "HEREDOC_PLACEHOLDER_" + std::to_string(next_placeholder_id());

    std::string rest_of_line = result.substr(delim_end, content_start - delim_end);

    std::string stored_content = content;
    if (!delimiter_quoted) {
        stored_content = "__EXPAND__" + content;
    }
    here_docs[placeholder] = stored_content;

    std::string before_here = result.substr(0, here_pos);
    std::string after_delimiter = result.substr(delimiter_line_end);

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

std::uint32_t CommandPreprocessor::next_placeholder_id() {
    static std::uint32_t counter = 0;
    if (counter == std::numeric_limits<std::uint32_t>::max()) {
        counter = 0;
    }
    return ++counter;
}
