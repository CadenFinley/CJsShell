#include "suggestion_utils.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "builtin.h"
#include "cjsh_filesystem.h"
#include "shell.h"
#include "shell_script_interpreter.h"

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

    if (suggestions.empty()) {
        suggestions.push_back("Try 'help' to see available commands.");
    }

    return suggestions;
}

std::vector<std::string> generate_cd_suggestions(const std::string& target_dir,
                                                 const std::string& current_dir) {
    std::vector<std::string> suggestions;

    std::vector<std::string> similar = find_similar_entries(target_dir, current_dir, 5);

    suggestions.reserve(similar.size());
    for (const auto& dir : similar) {
        suggestions.push_back("Did you mean 'cd " + dir + "'?");
    }

    if (target_dir.find('/') == std::string::npos) {
        suggestions.push_back("Try 'ls' to see available directories.");
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

std::vector<std::string> generate_ls_suggestions(const std::string& path,
                                                 const std::string& current_dir) {
    std::vector<std::string> suggestions;

    std::string directory = current_dir;
    std::string filename = path;

    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        directory = path.substr(0, last_slash);
        filename = path.substr(last_slash + 1);
        if (directory.empty())
            directory = "/";
    }

    std::vector<std::string> similar = find_similar_entries(filename, directory, 3);

    for (const auto& item : similar) {
        std::string suggestion;
        if (last_slash != std::string::npos) {
            suggestion = "Did you mean 'ls ";
            suggestion += directory;
            suggestion += "/";
            suggestion += item;
            suggestion += "'?";
        } else {
            suggestion = "Did you mean 'ls ";
            suggestion += item;
            suggestion += "'?";
        }
        suggestions.push_back(suggestion);
    }

    if (suggestions.empty()) {
        suggestions.push_back("Try 'ls' to see available files and directories.");
        if (path.find('/') != std::string::npos) {
            suggestions.push_back("Check if the directory path exists.");
        }
        suggestions.push_back("Use 'ls -la' to see hidden files.");
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

    try {
        std::vector<std::pair<int, std::string>> candidates;

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            std::string name = entry.path().filename().string();

            if (name[0] == '.' && target_name[0] != '.') {
                continue;
            }

            int distance = edit_distance(target_name, name);
            int score = 0;

            int max_distance =
                std::max(3, static_cast<int>(std::max(target_name.length(), name.length()) / 2));

            if (distance <= max_distance && distance > 0) {
                score = 1000 - distance * 10;

                if (!target_name.empty() && !name.empty() &&
                    std::tolower(target_name[0]) == std::tolower(name[0])) {
                    score += 50;
                }

                std::string target_lower = target_name;
                std::string name_lower = name;
                std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(),
                               ::tolower);
                std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

                if (name_lower.find(target_lower) != std::string::npos) {
                    score += 100;
                }

                int consecutive_matches = 0;
                size_t name_idx = 0;
                for (size_t i = 0; i < target_lower.length() && name_idx < name_lower.length();
                     i++) {
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

std::vector<std::string> generate_executable_suggestions(
    const std::string& command, const std::unordered_set<std::string>& available_commands) {
    std::vector<std::string> suggestions;

    if (command.length() < 2) {
        return suggestions;
    }

    std::vector<std::pair<int, std::string>> candidates;

    for (const auto& exec_name : available_commands) {
        int distance = edit_distance(command, exec_name);

        if (distance <= 3 && distance > 0) {
            int score = distance;

            if (!command.empty() && !exec_name.empty() &&
                std::tolower(command[0]) == std::tolower(exec_name[0])) {
                score -= 1;
            }

            if (exec_name.find(command) != std::string::npos) {
                score -= 2;
            }

            if (exec_name.length() <= command.length() + 2) {
                score -= 1;
            }

            candidates.emplace_back(std::max(1, score), exec_name);
        }
    }

    std::sort(candidates.begin(), candidates.end());

    for (size_t i = 0; i < candidates.size() && i < 5; i++) {
        suggestions.push_back("Did you mean '" + candidates[i].second + "'?");
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
                (seen_commands.count(cmd) == 0u)) {
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
        if (cmd == command || (seen_commands.count(cmd) != 0u))
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
        if (candidate_chars.count(ch) != 0u) {
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
