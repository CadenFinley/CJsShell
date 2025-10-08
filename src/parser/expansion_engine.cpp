#include "parser/expansion_engine.h"

#include <glob.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

#include "shell.h"

ExpansionEngine::ExpansionEngine(Shell* shell) : shell(shell) {
}

std::vector<std::string> ExpansionEngine::expand_braces(const std::string& pattern) {
    std::vector<std::string> result;

    size_t open_pos = pattern.find('{');
    if (open_pos == std::string::npos) {
        result.push_back(pattern);
        return result;
    }

    int depth = 1;
    size_t close_pos = open_pos + 1;

    while (close_pos < pattern.size() && depth > 0) {
        if (pattern[close_pos] == '{') {
            depth++;
        } else if (pattern[close_pos] == '}') {
            depth--;
        }

        if (depth > 0) {
            close_pos++;
        }
    }

    if (depth != 0) {
        result.push_back(pattern);
        return result;
    }

    std::string prefix = pattern.substr(0, open_pos);
    std::string content = pattern.substr(open_pos + 1, close_pos - open_pos - 1);
    std::string suffix = pattern.substr(close_pos + 1);

    if (content.empty() && prefix.empty() && suffix.empty()) {
        result.push_back(pattern);
        return result;
    }

    size_t range_pos = content.find("..");
    if (range_pos != std::string::npos) {
        std::string start_str = content.substr(0, range_pos);
        std::string end_str = content.substr(range_pos + 2);

        try {
            int start = std::stoi(start_str);
            int end = std::stoi(end_str);

            size_t range_size = std::abs(end - start) + 1;

            if (range_size > MAX_EXPANSION_SIZE) {
                result.push_back(pattern);
                return result;
            }

            result.reserve(range_size);

            if (start <= end) {
                for (int i = start; i <= end; ++i) {
                    std::string combined = prefix;
                    combined += std::to_string(i);
                    combined += suffix;
                    std::vector<std::string> expanded_results = expand_braces(combined);
                    result.insert(result.end(), std::make_move_iterator(expanded_results.begin()),
                                  std::make_move_iterator(expanded_results.end()));
                }
            } else {
                for (int i = start; i >= end; --i) {
                    std::string combined = prefix;
                    combined += std::to_string(i);
                    combined += suffix;
                    std::vector<std::string> expanded_results = expand_braces(combined);
                    result.insert(result.end(), std::make_move_iterator(expanded_results.begin()),
                                  std::make_move_iterator(expanded_results.end()));
                }
            }
            return result;
        } catch (const std::exception&) {
            if (start_str.length() == 1 && end_str.length() == 1 &&
                (std::isalpha(start_str[0]) != 0) && (std::isalpha(end_str[0]) != 0)) {
                char start_char = start_str[0];
                char end_char = end_str[0];

                size_t char_range_size = std::abs(end_char - start_char) + 1;

                if (char_range_size > MAX_EXPANSION_SIZE) {
                    result.push_back(pattern);
                    return result;
                }

                result.reserve(char_range_size);

                if (start_char <= end_char) {
                    for (char c = start_char; c <= end_char; ++c) {
                        std::string combined = prefix;
                        combined.append(1, c);
                        combined.append(suffix);
                        std::vector<std::string> expanded_results = expand_braces(combined);
                        result.insert(result.end(),
                                      std::make_move_iterator(expanded_results.begin()),
                                      std::make_move_iterator(expanded_results.end()));
                    }
                } else {
                    for (char c = start_char; c >= end_char; --c) {
                        std::string combined = prefix;
                        combined.append(1, c);
                        combined.append(suffix);
                        std::vector<std::string> expanded_results = expand_braces(combined);
                        result.insert(result.end(),
                                      std::make_move_iterator(expanded_results.begin()),
                                      std::make_move_iterator(expanded_results.end()));
                    }
                }
                return result;
            }
        }
    }

    std::vector<std::string> options;
    size_t start = 0;
    depth = 0;

    for (size_t i = 0; i <= content.size(); ++i) {
        if (i == content.size() || (content[i] == ',' && depth == 0)) {
            options.emplace_back(content.substr(start, i - start));
            start = i + 1;
        } else if (content[i] == '{') {
            depth++;
        } else if (content[i] == '}') {
            depth--;
        }
    }

    if (options.size() > MAX_EXPANSION_SIZE) {
        result.push_back(pattern);
        return result;
    }

    result.reserve(options.size());

    for (const auto& option : options) {
        std::string combined = prefix;
        combined.append(option);
        combined.append(suffix);
        std::vector<std::string> expanded_results = expand_braces(combined);
        result.insert(result.end(), std::make_move_iterator(expanded_results.begin()),
                      std::make_move_iterator(expanded_results.end()));
    }

    return result;
}

std::vector<std::string> ExpansionEngine::expand_wildcards(const std::string& pattern) {
    std::vector<std::string> result;

    if (shell != nullptr && shell->get_shell_option("noglob")) {
        result.push_back(pattern);
        return result;
    }

    bool has_wildcards = false;

    for (size_t i = 0; i < pattern.length(); ++i) {
        char c = pattern[i];

        if (c == '\x1F' && i + 1 < pattern.length()) {
            i++;
            continue;
        }

        if (c == '*' || c == '?' || c == '[') {
            has_wildcards = true;
            break;
        }
    }

    std::string unescaped;
    unescaped.reserve(pattern.length());

    for (size_t i = 0; i < pattern.length(); ++i) {
        if (pattern[i] == '\x1F') {
            if (i + 1 < pattern.length()) {
                i++;
                unescaped += pattern[i];
            }
        } else {
            unescaped += pattern[i];
        }
    }

    if (!has_wildcards) {
        result.push_back(unescaped);
        return result;
    }

    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    int return_value = glob(unescaped.c_str(), GLOB_TILDE | GLOB_MARK, nullptr, &glob_result);
    if (return_value == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            result.push_back(std::string(glob_result.gl_pathv[i]));
        }
        globfree(&glob_result);
    } else if (return_value == GLOB_NOMATCH) {
        result.push_back(unescaped);
    }

    return result;
}
