#include "suggestion_utils.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../utils/cjsh_syntax_highlighter.h"
#include "cjsh_filesystem.h"

namespace suggestion_utils {

std::vector<std::string> generate_command_suggestions(
    const std::string& command) {
  std::vector<std::string> suggestions;

  // Start with essential common typos for the most frequent mistakes
  static const std::unordered_map<std::string, std::vector<std::string>>
      essential_typos = {
          // Basic navigation
          {"sl", {"ls"}},
          {"ll", {"ls -l"}},
          {"la", {"ls -la"}},
          {"ks", {"ls"}},
          {"dir", {"ls"}},
          {"cls", {"clear"}},

          // Basic commands
          {"ehco", {"echo"}},
          {"exot", {"exit"}},
          {"eixt", {"exit"}},
          {"quit", {"exit"}},
          {"clea", {"clear"}},
          {"clera", {"clear"}},
          {"whcih", {"which"}},
          {"wich", {"which"}},

          // Version control
          {"gii", {"git"}},
          {"gitt", {"git"}},
          {"tig", {"git"}},

          // Common utilities
          {"grpe", {"grep"}},
          {"gerp", {"grep"}},
          {"sudp", {"sudo"}},
          {"sduo", {"sudo"}},
          {"crul", {"curl"}},
          {"wgte", {"wget"}},
          {"vmi", {"vim"}},
          {"ivm", {"vim"}},
          {"tarr", {"tar"}},
          {"unizp", {"unzip"}},
          {"gizp", {"gzip"}},
          {"gunizp", {"gunzip"}},
          {"findd", {"find"}},
          {"whoa", {"whoami"}},
          {"hisotry", {"history"}},
          {"basrh", {"bash"}},
          {"zhs", {"zsh"}},
          {"shh", {"ssh"}},
          {"rsyn", {"rsync"}},
      };

  // Check essential typos first
  auto it = essential_typos.find(command);
  if (it != essential_typos.end()) {
    for (const auto& suggestion : it->second) {
      suggestions.push_back("Did you mean: " + suggestion);
    }
    return suggestions;
  }

  // Create comprehensive command list: builtins + executables + common commands
  std::unordered_set<std::string>
      all_commands_set;  // Use set to avoid duplicates

  // Add shell builtins (highest priority) - from cjsh
  static const std::vector<std::string> shell_builtins = {
      "echo",        "printf",  "pwd",     "cd",     "ls",       "alias",
      "export",      "unalias", "unset",   "set",    "shift",    "break",
      "continue",    "return",  "ai",      "source", ".",        "theme",
      "plugin",      "help",    "approot", "aihelp", "version",  "uninstall",
      "eval",        "syntax",  "history", "exit",   "quit",     "terminal",
      "prompt_test", "test",    "[",       "exec",   "trap",     "jobs",
      "fg",          "bg",      "wait",    "kill",   "readonly", "read",
      "umask",       "getopts", "times",   "type",   "hash"};

  for (const auto& builtin : shell_builtins) {
    all_commands_set.insert(builtin);
  }

  // Add cached executables from PATH
  auto cached_executables = cjsh_filesystem::read_cached_executables();
  for (const auto& exec_path : cached_executables) {
    all_commands_set.insert(exec_path.filename().string());
  }

  // Add common commands not already included
  std::vector<std::string> common_commands = {
      "man",     "info",    "sudo",   "su",      "which",   "whereis",
      "locate",  "find",    "grep",   "egrep",   "fgrep",   "sed",
      "awk",     "cut",     "sort",   "uniq",    "wc",      "head",
      "tail",    "less",    "more",   "cat",     "tac",     "rev",
      "tr",      "paste",   "join",   "comm",    "diff",    "cmp",
      "file",    "stat",    "touch",  "cp",      "mv",      "rm",
      "rmdir",   "mkdir",   "ln",     "chmod",   "chown",   "chgrp",
      "umask",   "tar",     "gzip",   "gunzip",  "zip",     "unzip",
      "bzip2",   "bunzip2", "xz",     "unxz",    "curl",    "wget",
      "ping",    "ssh",     "scp",    "rsync",   "ftp",     "sftp",
      "ps",      "top",     "htop",   "kill",    "killall", "jobs",
      "bg",      "fg",      "nohup",  "df",      "du",      "mount",
      "umount",  "fdisk",   "free",   "uptime",  "uname",   "whoami",
      "who",     "w",       "last",   "finger",  "id",      "groups",
      "su",      "sudo",    "git",    "svn",     "hg",      "bzr",
      "cvs",     "make",    "cmake",  "gcc",     "g++",     "clang",
      "python",  "python3", "node",   "npm",     "ruby",    "perl",
      "java",    "javac",   "docker", "kubectl", "helm",    "terraform",
      "ansible", "vagrant", "vim",    "vi",      "nano",    "emacs",
      "less",    "more",    "cat"};

  // Add common commands (set automatically handles duplicates)
  for (const auto& cmd : common_commands) {
    all_commands_set.insert(cmd);
  }

  // Convert set back to vector for fuzzy matching
  std::vector<std::string> all_commands(all_commands_set.begin(),
                                        all_commands_set.end());

  // Generate fuzzy suggestions
  suggestions = generate_fuzzy_suggestions(command, all_commands);

  // If still no suggestions, provide helpful fallback
  if (suggestions.empty()) {
    suggestions.push_back("Try 'help' to see available commands.");
  }

  return suggestions;
}

std::vector<std::string> generate_cd_suggestions(
    const std::string& target_dir, const std::string& current_dir) {
  std::vector<std::string> suggestions;

  // Find similar directory names in current directory
  std::vector<std::string> similar =
      find_similar_entries(target_dir, current_dir, 3);

  for (const auto& dir : similar) {
    suggestions.push_back("Did you mean 'cd " + dir + "'?");
  }

  // Common cd suggestions
  if (target_dir.find('/') == std::string::npos) {
    // Single directory name - suggest looking in common locations
    suggestions.push_back("Try 'ls' to see available directories.");
    if (target_dir != "..") {
      suggestions.push_back("Use 'cd ..' to go to parent directory.");
    }
  } else {
    // Path with slashes - suggest checking intermediate directories
    std::string parent_path =
        target_dir.substr(0, target_dir.find_last_of('/'));
    if (!parent_path.empty() && parent_path != target_dir) {
      suggestions.push_back("Check if '" + parent_path + "' exists first.");
    }
  }

  return suggestions;
}

std::vector<std::string> generate_ls_suggestions(
    const std::string& path, const std::string& current_dir) {
  std::vector<std::string> suggestions;

  // Extract directory and filename
  std::string directory = current_dir;
  std::string filename = path;

  size_t last_slash = path.find_last_of('/');
  if (last_slash != std::string::npos) {
    directory = path.substr(0, last_slash);
    filename = path.substr(last_slash + 1);
    if (directory.empty())
      directory = "/";
  }

  // Find similar files/directories
  std::vector<std::string> similar =
      find_similar_entries(filename, directory, 3);

  for (const auto& item : similar) {
    if (last_slash != std::string::npos) {
      suggestions.push_back("Did you mean 'ls " + directory + "/" + item +
                            "'?");
    } else {
      suggestions.push_back("Did you mean 'ls " + item + "'?");
    }
  }

  // General suggestions
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
                                              const std::string& directory,
                                              int max_suggestions) {
  std::vector<std::string> suggestions;

  try {
    std::vector<std::pair<int, std::string>> candidates;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
      std::string name = entry.path().filename().string();

      // Skip hidden files starting with '.' unless target also starts with '.'
      if (name[0] == '.' && target_name[0] != '.') {
        continue;
      }

      int distance = edit_distance(target_name, name);

      // Only consider entries with reasonable similarity
      if (distance <= 3 && distance > 0) {
        candidates.emplace_back(distance, name);
      }
    }

    // Sort by edit distance (closest matches first)
    std::sort(candidates.begin(), candidates.end());

    // Add the best matches up to max_suggestions
    for (size_t i = 0;
         i < candidates.size() && i < static_cast<size_t>(max_suggestions);
         i++) {
      suggestions.push_back(candidates[i].second);
    }

  } catch (const std::filesystem::filesystem_error&) {
    // Directory doesn't exist or can't be read - no suggestions
  }

  return suggestions;
}

// Helper function to generate suggestions based on available executables
std::vector<std::string> generate_executable_suggestions(
    const std::string& command,
    const std::unordered_set<std::string>& available_commands) {
  std::vector<std::string> suggestions;

  if (command.length() < 2) {
    return suggestions;
  }

  // Load cached suggestions if available
  auto cached_suggestions = load_cached_suggestions(command);
  if (!cached_suggestions.empty()) {
    return cached_suggestions;
  }

  // Create a vector of candidates with their edit distances
  std::vector<std::pair<int, std::string>> candidates;

  for (const auto& exec_name : available_commands) {
    int distance = edit_distance(command, exec_name);

    // Consider commands with reasonable similarity
    if (distance <= 3 && distance > 0) {
      // Add bonus scoring for common patterns
      int score = distance;

      // Bonus for commands that start with the same letter
      if (!command.empty() && !exec_name.empty() &&
          std::tolower(command[0]) == std::tolower(exec_name[0])) {
        score -= 1;
      }

      // Bonus for commands that contain the input as substring
      if (exec_name.find(command) != std::string::npos) {
        score -= 2;
      }

      // Bonus for shorter commands (more likely to be what user wants)
      if (exec_name.length() <= command.length() + 2) {
        score -= 1;
      }

      candidates.emplace_back(std::max(1, score), exec_name);
    }
  }

  // Sort by score (lower is better)
  std::sort(candidates.begin(), candidates.end());

  // Add the best matches
  for (size_t i = 0; i < candidates.size() && i < 5; i++) {
    suggestions.push_back("Did you mean '" + candidates[i].second + "'?");
  }

  // Cache the suggestions for future use
  cache_suggestions(command, suggestions);

  return suggestions;
}

// Helper function to load cached suggestions
std::vector<std::string> load_cached_suggestions(const std::string& command) {
  std::vector<std::string> suggestions;

  try {
    auto cache_file =
        cjsh_filesystem::g_cjsh_cache_path / "command_suggestions.cache";

    if (!std::filesystem::exists(cache_file)) {
      return suggestions;
    }

    // Check if cache is recent (less than 1 day old)
    auto last_write = std::filesystem::last_write_time(cache_file);
    auto now = decltype(last_write)::clock::now();
    if ((now - last_write) > std::chrono::hours(24)) {
      return suggestions;  // Cache is too old
    }

    std::ifstream cache_stream(cache_file);
    std::string line;
    bool found_command = false;

    while (std::getline(cache_stream, line)) {
      if (line == "CMD:" + command) {
        found_command = true;
        continue;
      }

      if (found_command) {
        if (line.empty() || line.substr(0, 4) == "CMD:") {
          break;  // End of this command's suggestions
        }
        suggestions.push_back(line);
      }
    }
  } catch (const std::exception&) {
    // If anything goes wrong, return empty suggestions
  }

  return suggestions;
}

// Helper function to cache suggestions
void cache_suggestions(const std::string& command,
                       const std::vector<std::string>& suggestions) {
  try {
    auto cache_file =
        cjsh_filesystem::g_cjsh_cache_path / "command_suggestions.cache";

    // Read existing cache
    std::unordered_map<std::string, std::vector<std::string>> cache_data;

    if (std::filesystem::exists(cache_file)) {
      std::ifstream cache_stream(cache_file);
      std::string line;
      std::string current_command;

      while (std::getline(cache_stream, line)) {
        if (line.substr(0, 4) == "CMD:") {
          current_command = line.substr(4);
          cache_data[current_command] = std::vector<std::string>();
        } else if (!line.empty() && !current_command.empty()) {
          cache_data[current_command].push_back(line);
        }
      }
    }

    // Update with new suggestions
    cache_data[command] = suggestions;

    // Write updated cache
    std::ofstream cache_stream(cache_file);
    for (const auto& entry : cache_data) {
      cache_stream << "CMD:" << entry.first << "\n";
      for (const auto& suggestion : entry.second) {
        cache_stream << suggestion << "\n";
      }
      cache_stream << "\n";
    }

  } catch (const std::exception&) {
    // If caching fails, just continue without caching
  }
}

// Function to analyze command usage patterns and update suggestions
void update_command_usage_stats(const std::string& command) {
  try {
    auto stats_file =
        cjsh_filesystem::g_cjsh_cache_path / "command_usage_stats.cache";

    // Read existing stats
    std::unordered_map<std::string, int> usage_stats;

    if (std::filesystem::exists(stats_file)) {
      std::ifstream stats_stream(stats_file);
      std::string line;

      while (std::getline(stats_stream, line)) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
          std::string cmd = line.substr(0, pos);
          int count = std::stoi(line.substr(pos + 1));
          usage_stats[cmd] = count;
        }
      }
    }

    // Increment usage for this command
    usage_stats[command]++;

    // Write updated stats
    std::ofstream stats_stream(stats_file);
    for (const auto& entry : usage_stats) {
      stats_stream << entry.first << ":" << entry.second << "\n";
    }

  } catch (const std::exception&) {
    // If updating stats fails, just continue
  }
}

// Function to get command usage frequency for ranking
int get_command_usage_frequency(const std::string& command) {
  try {
    auto stats_file =
        cjsh_filesystem::g_cjsh_cache_path / "command_usage_stats.cache";

    if (!std::filesystem::exists(stats_file)) {
      return 0;
    }

    std::ifstream stats_stream(stats_file);
    std::string line;

    while (std::getline(stats_stream, line)) {
      size_t pos = line.find(':');
      if (pos != std::string::npos) {
        std::string cmd = line.substr(0, pos);
        if (cmd == command) {
          return std::stoi(line.substr(pos + 1));
        }
      }
    }
  } catch (const std::exception&) {
    // If reading fails, return 0
  }

  return 0;
}

// Fuzzy matching function to generate intelligent suggestions
std::vector<std::string> generate_fuzzy_suggestions(
    const std::string& command,
    const std::vector<std::string>& available_commands) {
  std::vector<std::string> suggestions;

  if (command.empty()) {
    return suggestions;
  }

  // Special handling for single letter commands - only suggest commands that
  // start with that letter
  if (command.length() == 1) {
    std::vector<std::pair<int, std::string>> single_letter_candidates;
    std::unordered_set<std::string> seen_commands;
    char target_char = std::tolower(command[0]);

    for (const auto& cmd : available_commands) {
      if (!cmd.empty() && std::tolower(cmd[0]) == target_char &&
          !seen_commands.count(cmd)) {
        // Prioritize shell builtins and common commands
        int priority = 0;
        if (cmd == "ls" || cmd == "cd" || cmd == "ps" || cmd == "cp" ||
            cmd == "mv") {
          priority = 100;  // Very common commands
        } else if (cmd.length() <= 4) {
          priority = 50;  // Short commands are more likely
        } else {
          priority = 10;  // Longer commands get lower priority
        }

        single_letter_candidates.emplace_back(priority, cmd);
        seen_commands.insert(cmd);
      }
    }

    // Sort by priority (higher is better)
    std::sort(single_letter_candidates.begin(), single_letter_candidates.end(),
              std::greater<std::pair<int, std::string>>());

    // Add the best matches (limit to 5)
    for (size_t i = 0; i < single_letter_candidates.size() && i < 5; i++) {
      suggestions.push_back("Did you mean '" +
                            single_letter_candidates[i].second + "'?");
    }

    return suggestions;
  }

  // Load cached suggestions if available
  auto cached_suggestions = load_cached_suggestions(command);
  if (!cached_suggestions.empty()) {
    return cached_suggestions;
  }

  // Create candidates with scoring
  std::vector<std::pair<int, std::string>> candidates;
  std::unordered_set<std::string> seen_commands;  // Track to avoid duplicates

  for (const auto& cmd : available_commands) {
    // Skip if command is identical or already seen
    if (cmd == command || seen_commands.count(cmd))
      continue;

    int score = calculate_fuzzy_score(command, cmd);

    // Only consider commands with reasonable similarity
    if (score > 0) {
      candidates.emplace_back(score, cmd);
      seen_commands.insert(cmd);
    }
  }

  // Sort by score (higher is better)
  std::sort(candidates.begin(), candidates.end(),
            std::greater<std::pair<int, std::string>>());

  // Add the best matches (limit to 5)
  for (size_t i = 0; i < candidates.size() && i < 5; i++) {
    suggestions.push_back("Did you mean '" + candidates[i].second + "'?");
  }

  // Cache the suggestions for future use
  cache_suggestions(command, suggestions);

  return suggestions;
}

// Calculate fuzzy matching score (higher = better match)
int calculate_fuzzy_score(const std::string& input,
                          const std::string& candidate) {
  if (input.empty() || candidate.empty())
    return 0;

  // Exact match gets highest score
  if (input == candidate)
    return 1000;

  // Calculate base edit distance (lower is better)
  int distance = edit_distance(input, candidate);

  // Reject candidates that are too different
  int max_distance = std::max(2, static_cast<int>(input.length()) / 2);
  if (distance > max_distance)
    return 0;

  // Start with base score (inversely related to edit distance)
  int score = 100 - (distance * 20);

  // Bonus scoring factors:

  // 1. Same starting character (very important for command completion)
  if (std::tolower(input[0]) == std::tolower(candidate[0])) {
    score += 30;
  }

  // 2. Input is a prefix of candidate (common for partial typing)
  if (candidate.length() >= input.length() &&
      candidate.substr(0, input.length()) == input) {
    score += 40;
  }

  // 3. Input is contained within candidate
  if (candidate.find(input) != std::string::npos) {
    score += 25;
  }

  // 4. Similar length (prefer commands of similar length)
  int length_diff = std::abs(static_cast<int>(input.length()) -
                             static_cast<int>(candidate.length()));
  if (length_diff <= 2) {
    score += 15;
  }

  // 5. Common character patterns (check for transpositions)
  int common_chars = 0;
  std::unordered_map<char, int> input_chars, candidate_chars;
  for (char c : input)
    input_chars[std::tolower(c)]++;
  for (char c : candidate)
    candidate_chars[std::tolower(c)]++;

  for (const auto& pair : input_chars) {
    char ch = pair.first;
    int count = pair.second;
    if (candidate_chars.count(ch)) {
      common_chars += std::min(count, candidate_chars[ch]);
    }
  }

  // Bonus for high character overlap
  double char_overlap = static_cast<double>(common_chars) /
                        std::max(input.length(), candidate.length());
  score += static_cast<int>(char_overlap * 20);

  // 6. Penalty for very long candidates when input is short
  if (input.length() <= 3 && candidate.length() > 8) {
    score -= 10;
  }

  // 7. Bonus for shell builtins (they should be prioritized)
  static const std::unordered_set<std::string> shell_builtins = {
      "echo",        "printf",  "pwd",     "cd",     "ls",       "alias",
      "export",      "unalias", "unset",   "set",    "shift",    "break",
      "continue",    "return",  "ai",      "source", ".",        "theme",
      "plugin",      "help",    "approot", "aihelp", "version",  "uninstall",
      "eval",        "syntax",  "history", "exit",   "quit",     "terminal",
      "prompt_test", "test",    "[",       "exec",   "trap",     "jobs",
      "fg",          "bg",      "wait",    "kill",   "readonly", "read",
      "umask",       "getopts", "times",   "type",   "hash"};

  if (shell_builtins.count(candidate)) {
    score += 15;
  }

  return std::max(0, score);
}

}  // namespace suggestion_utils