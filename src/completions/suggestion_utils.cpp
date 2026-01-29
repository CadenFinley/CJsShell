#include "suggestion_utils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "builtin.h"
#include "cjsh_filesystem.h"
#include "interpreter.h"
#include "shell.h"

extern std::unique_ptr<Shell> g_shell;
namespace suggestion_utils {

std::vector<std::string> generate_command_suggestions(const std::string& command) {
    std::vector<std::string> suggestions;

    std::unordered_set<std::string> all_commands_set;

    if (g_shell && g_shell->get_built_ins()) {
        auto builtin_commands = g_shell->get_built_ins()->get_builtin_commands();
        for (const auto& builtin : builtin_commands) {
            all_commands_set.insert(builtin);
        }
    }

    if (g_shell) {
        auto& aliases = g_shell->get_aliases();
        for (const auto& alias_pair : aliases) {
            all_commands_set.insert(alias_pair.first);
        }
    }

    if (g_shell) {
        auto& abbreviations = g_shell->get_abbreviations();
        for (const auto& abbr_pair : abbreviations) {
            all_commands_set.insert(abbr_pair.first);
        }
    }

    if (g_shell && g_shell->get_shell_script_interpreter()) {
        auto function_names = g_shell->get_shell_script_interpreter()->get_function_names();
        for (const auto& func_name : function_names) {
            all_commands_set.insert(func_name);
        }
    }

    auto executables = cjsh_filesystem::get_executables_in_path();
    for (const auto& exec_name : executables) {
        all_commands_set.insert(exec_name);
    }

    std::vector<std::string> all_commands(all_commands_set.begin(), all_commands_set.end());

    suggestions = generate_fuzzy_suggestions(command, all_commands);
    return suggestions;
}

std::vector<std::string> generate_cd_suggestions(const std::string& target_dir,
                                                 const std::string& current_dir) {
    std::vector<std::string> suggestions;

    std::filesystem::path current_path(current_dir);
    std::filesystem::path target_path(target_dir);
    std::filesystem::path base_path = current_path;
    std::string lookup_fragment = target_dir;

    std::error_code ec;

    if (!target_dir.empty()) {
        if (target_path.is_absolute()) {
            base_path = target_path.parent_path();
            lookup_fragment = target_path.filename().string();
            if (lookup_fragment.empty()) {
                lookup_fragment = target_path.string();
            }
        } else {
            std::filesystem::path resolved = current_path / target_path;
            std::filesystem::path parent = resolved.parent_path();
            if (parent.empty()) {
                parent = current_path;
            }

            if (std::filesystem::exists(parent, ec)) {
                base_path = parent;
                lookup_fragment = resolved.filename().string();
                if (lookup_fragment.empty()) {
                    lookup_fragment = target_path.filename().string();
                    if (lookup_fragment.empty()) {
                        lookup_fragment = target_dir;
                    }
                }
            } else {
                ec.clear();
            }
        }
    }

    if (lookup_fragment.empty()) {
        lookup_fragment = target_dir;
    }

    std::string base_dir = base_path.empty() ? current_dir : base_path.string();

    auto raw_similar = find_similar_entries(lookup_fragment, base_dir, 5);

    std::filesystem::path search_base =
        base_path.empty() ? std::filesystem::path(base_dir) : base_path;

    std::vector<std::string> similar;
    std::unordered_set<std::string> seen_paths;

    for (const auto& candidate : raw_similar) {
        std::filesystem::path candidate_path = search_base / candidate;

        if (!std::filesystem::exists(candidate_path, ec)) {
            ec.clear();
            continue;
        }

        if (!std::filesystem::is_directory(candidate_path, ec)) {
            ec.clear();
            continue;
        }

        std::string display_value;
        if (target_path.is_absolute()) {
            display_value = candidate_path.string();
        } else {
            auto relative_candidate = std::filesystem::relative(candidate_path, current_path, ec);
            if (!ec && !relative_candidate.empty() && relative_candidate.string() != ".") {
                display_value = relative_candidate.string();
            } else {
                ec.clear();
                display_value = candidate_path.string();
            }
        }

        if (display_value.empty()) {
            display_value = candidate;
        }

        if (seen_paths.insert(display_value).second) {
            similar.push_back(display_value);
        }
    }

    suggestions.reserve(similar.size());
    for (const auto& dir : similar) {
        suggestions.push_back("Did you mean 'cd " + dir + "'?");
    }

    if (target_dir.find('/') == std::string::npos) {
        if (similar.empty()) {
            suggestions.push_back("Try 'ls' to see available directories.");
        }
        if (target_dir != "..") {
            suggestions.push_back("Use 'cd ..' to go to parent directory.");
        }
    } else {
        std::string parent_path = target_dir.substr(0, target_dir.find_last_of('/'));
        if (!parent_path.empty() && parent_path != target_dir) {
            suggestions.push_back("Check if '" + parent_path + "' exists first.");
        }
    }

    return suggestions;
}

int edit_distance(const std::string& str1, const std::string& str2) {
    const size_t m = str1.length();
    const size_t n = str2.length();

    if (m == 0)
        return n;
    if (n == 0)
        return m;

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (size_t i = 0; i <= m; i++)
        dp[i][0] = i;
    for (size_t j = 0; j <= n; j++)
        dp[0][j] = j;

    for (size_t i = 1; i <= m; i++) {
        for (size_t j = 1; j <= n; j++) {
            if (str1[i - 1] == str2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }

    return dp[m][n];
}

std::vector<std::string> find_similar_entries(const std::string& target_name,
                                              const std::string& directory, int max_suggestions) {
    std::vector<std::string> suggestions;

    if (target_name.empty()) {
        return suggestions;
    }

    std::string target_lower = target_name;
    std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    try {
        std::vector<std::pair<int, std::string>> candidates;

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            std::string name = entry.path().filename().string();

            if (name.empty()) {
                continue;
            }

            if (name[0] == '.' && target_name[0] != '.') {
                continue;
            }

            std::string name_lower = name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            bool substring_match = name_lower.find(target_lower) != std::string::npos;

            int distance = edit_distance(target_name, name);
            int max_distance =
                std::max(3, static_cast<int>(std::max(target_name.length(), name.length()) / 2));

            if (!substring_match && (distance == 0 || distance > max_distance)) {
                continue;
            }

            int score = 1000 - distance * 10;

            if (substring_match) {
                score += 200;
                if (name_lower.rfind(target_lower, 0) == 0) {
                    score += 150;
                }
                score -= static_cast<int>(
                    std::max<size_t>(0, name_lower.length() - target_lower.length()));
            }

            if (std::tolower(static_cast<unsigned char>(target_name[0])) ==
                std::tolower(static_cast<unsigned char>(name[0]))) {
                score += 50;
            }

            int consecutive_matches = 0;
            size_t name_idx = 0;
            for (size_t i = 0; i < target_lower.length() && name_idx < name_lower.length(); i++) {
                bool found = false;
                for (size_t j = name_idx; j < name_lower.length(); j++) {
                    if (target_lower[i] == name_lower[j]) {
                        consecutive_matches++;
                        name_idx = j + 1;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    break;
            }

            if (consecutive_matches >= static_cast<int>(target_name.length() * 0.8)) {
                score += 200;
            }

            candidates.emplace_back(score, name);
        }

        std::sort(candidates.begin(), candidates.end(),
                  std::greater<std::pair<int, std::string>>());

        for (size_t i = 0; i < candidates.size() && i < static_cast<size_t>(max_suggestions); i++) {
            suggestions.push_back(candidates[i].second);
        }

    } catch (const std::filesystem::filesystem_error&) {
    }

    return suggestions;
}

std::vector<std::string> generate_fuzzy_suggestions(
    const std::string& command, const std::vector<std::string>& available_commands) {
    std::vector<std::string> suggestions;

    if (command.empty()) {
        return suggestions;
    }

    if (command.length() == 1) {
        std::vector<std::pair<int, std::string>> single_letter_candidates;
        std::unordered_set<std::string> seen_commands;
        char target_char = std::tolower(command[0]);

        for (const auto& cmd : available_commands) {
            if (!cmd.empty() && std::tolower(cmd[0]) == target_char &&
                (seen_commands.count(cmd) == 0U)) {
                int priority = 0;
                if (cmd == "ls" || cmd == "cd" || cmd == "ps" || cmd == "cp" || cmd == "mv") {
                    priority = 100;
                } else if (cmd.length() <= 4) {
                    priority = 50;
                } else {
                    priority = 10;
                }

                single_letter_candidates.emplace_back(priority, cmd);
                seen_commands.insert(cmd);
            }
        }

        std::sort(single_letter_candidates.begin(), single_letter_candidates.end(),
                  std::greater<std::pair<int, std::string>>());

        for (size_t i = 0; i < single_letter_candidates.size() && i < 5; i++) {
            suggestions.push_back("Did you mean '" + single_letter_candidates[i].second + "'?");
        }

        return suggestions;
    }

    std::vector<std::pair<int, std::string>> candidates;
    std::unordered_set<std::string> seen_commands;

    for (const auto& cmd : available_commands) {
        if (cmd == command || (seen_commands.count(cmd) != 0U))
            continue;

        int score = calculate_fuzzy_score(command, cmd);

        if (score > 0) {
            candidates.emplace_back(score, cmd);
            seen_commands.insert(cmd);
        }
    }

    std::sort(candidates.begin(), candidates.end(), std::greater<std::pair<int, std::string>>());

    for (size_t i = 0; i < candidates.size() && i < 5; i++) {
        suggestions.push_back("Did you mean '" + candidates[i].second + "'?");
    }

    return suggestions;
}

int calculate_fuzzy_score(const std::string& input, const std::string& candidate) {
    if (input.empty() || candidate.empty())
        return 0;

    if (input == candidate)
        return 1000;

    int distance = edit_distance(input, candidate);

    int max_distance = std::max(2, static_cast<int>(input.length()) / 2);
    if (distance > max_distance)
        return 0;

    int score = 100 - (distance * 20);

    if (std::tolower(input[0]) == std::tolower(candidate[0])) {
        score += 30;
    }

    if (candidate.length() >= input.length() && candidate.substr(0, input.length()) == input) {
        score += 40;
    }

    if (candidate.find(input) != std::string::npos) {
        score += 25;
    }

    int length_diff =
        std::abs(static_cast<int>(input.length()) - static_cast<int>(candidate.length()));
    if (length_diff <= 2) {
        score += 15;
    }

    int common_chars = 0;
    std::unordered_map<char, int> input_chars;
    std::unordered_map<char, int> candidate_chars;
    for (char c : input)
        input_chars[std::tolower(c)]++;
    for (char c : candidate)
        candidate_chars[std::tolower(c)]++;

    for (const auto& pair : input_chars) {
        char ch = pair.first;
        int count = pair.second;
        if (candidate_chars.count(ch) != 0U) {
            common_chars += std::min(count, candidate_chars[ch]);
        }
    }

    double char_overlap =
        static_cast<double>(common_chars) / std::max(input.length(), candidate.length());
    score += static_cast<int>(char_overlap * 20);

    if (input.length() <= 3 && candidate.length() > 8) {
        score -= 10;
    }

    if (g_shell && g_shell->get_built_ins()) {
        auto builtin_commands = g_shell->get_built_ins()->get_builtin_commands();
        for (const auto& builtin : builtin_commands) {
            if (candidate == builtin) {
                score += 15;
                break;
            }
        }
    }

    return std::max(0, score);
}

}  // namespace suggestion_utils
