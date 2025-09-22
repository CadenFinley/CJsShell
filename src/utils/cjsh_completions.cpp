#include "cjsh_completions.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "cjsh_syntax_highlighter.h"
#include "isocline/isocline.h"
#include "shell.h"

std::map<std::string, int> g_completion_frequency;
enum CompletionContext {
  CONTEXT_COMMAND,
  CONTEXT_ARGUMENT,
  CONTEXT_PATH
};

// Completion deduplication system
struct CompletionTracker {
  std::unordered_set<std::string> added_completions;
  ic_completion_env_t* cenv;
  std::string original_prefix;
  
  CompletionTracker(ic_completion_env_t* env, const char* prefix) : cenv(env), original_prefix(prefix) {}
  
  // Calculate what the final result would look like after applying this completion
  std::string calculate_final_result(const char* completion_text, long delete_before = 0) {
    std::string prefix_str = original_prefix;
    
    // Calculate what would be left after deleting characters
    if (delete_before > 0 && delete_before <= static_cast<long>(prefix_str.length())) {
      prefix_str = prefix_str.substr(0, prefix_str.length() - delete_before);
    }
    
    // Add the completion text
    return prefix_str + completion_text;
  }
  
  // Check if this completion would result in the same final text as a previous one
  bool would_create_duplicate(const char* completion_text, long delete_before = 0) {
    std::string final_result = calculate_final_result(completion_text, delete_before);
    return added_completions.find(final_result) != added_completions.end();
  }
  
  // Wrapper for ic_add_completion that checks for duplicates
  bool add_completion_if_unique(const char* completion_text) {
    if (would_create_duplicate(completion_text, 0)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Skipping duplicate completion: '" << completion_text << "'" << std::endl;
      return true; // Don't fail, just skip the duplicate
    }
    
    std::string final_result = calculate_final_result(completion_text, 0);
    added_completions.insert(final_result);
    if (g_debug_mode)
      std::cerr << "DEBUG: Adding unique completion: '" << completion_text << "' -> final: '" << final_result << "'" << std::endl;
    return ic_add_completion(cenv, completion_text);
  }
  
  // Wrapper for ic_add_completion_prim that checks for duplicates
  bool add_completion_prim_if_unique(const char* completion_text, const char* display, const char* help, long delete_before, long delete_after) {
    if (would_create_duplicate(completion_text, delete_before)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Skipping duplicate completion (prim): '" << completion_text << "'" << std::endl;
      return true; // Don't fail, just skip the duplicate
    }
    
    std::string final_result = calculate_final_result(completion_text, delete_before);
    added_completions.insert(final_result);
    if (g_debug_mode)
      std::cerr << "DEBUG: Adding unique completion (prim): '" << completion_text << "' -> final: '" << final_result << "'" << std::endl;
    return ic_add_completion_prim(cenv, completion_text, display, help, delete_before, delete_after);
  }
};

// Thread-local completion tracker for the current completion session
thread_local CompletionTracker* g_current_completion_tracker = nullptr;

// Helper functions that automatically use deduplication when available
bool safe_add_completion(ic_completion_env_t* cenv, const char* completion_text) {
  if (g_current_completion_tracker) {
    return g_current_completion_tracker->add_completion_if_unique(completion_text);
  } else {
    return ic_add_completion(cenv, completion_text);
  }
}

bool safe_add_completion_prim(ic_completion_env_t* cenv, const char* completion_text, const char* display, const char* help, long delete_before, long delete_after) {
  if (g_current_completion_tracker) {
    return g_current_completion_tracker->add_completion_prim_if_unique(completion_text, display, help, delete_before, delete_after);
  } else {
    return ic_add_completion_prim(cenv, completion_text, display, help, delete_before, delete_after);
  }
}

// Helper function to find the last unquoted space in a string
size_t find_last_unquoted_space(const std::string& str) {
  bool in_single_quote = false;
  bool in_double_quote = false;
  bool escaped = false;

  for (int i = static_cast<int>(str.length()) - 1; i >= 0; --i) {
    char c = str[i];

    if (escaped) {
      escaped = false;
      continue;
    }

    if (c == '\\') {
      escaped = true;
      continue;
    }

    if (c == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
      continue;
    }

    if (c == '"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
      continue;
    }

    if ((c == ' ' || c == '\t') && !in_single_quote && !in_double_quote) {
      return static_cast<size_t>(i);
    }
  }

  return std::string::npos;
}

// Helper function to split command line into tokens respecting quotes and
// escapes
std::vector<std::string> tokenize_command_line(const std::string& line) {
  std::vector<std::string> tokens;
  std::string current_token;
  bool in_single_quote = false;
  bool in_double_quote = false;
  bool escaped = false;

  for (size_t i = 0; i < line.length(); ++i) {
    char c = line[i];

    if (escaped) {
      current_token += c;
      escaped = false;
      continue;
    }

    if (c == '\\') {
      if (in_single_quote) {
        current_token += c;  // In single quotes, backslash is literal
      } else {
        escaped = true;
      }
      continue;
    }

    if (c == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
      continue;
    }

    if (c == '"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
      continue;
    }

    if ((c == ' ' || c == '\t') && !in_single_quote && !in_double_quote) {
      if (!current_token.empty()) {
        tokens.push_back(current_token);
        current_token.clear();
      }
      continue;
    }

    current_token += c;
  }

  if (!current_token.empty()) {
    tokens.push_back(current_token);
  }

  return tokens;
}

// Helper function to unquote and unescape a path string
std::string unquote_path(const std::string& path) {
  if (path.empty())
    return path;

  std::string result;
  bool in_single_quote = false;
  bool in_double_quote = false;
  bool escaped = false;

  for (size_t i = 0; i < path.length(); ++i) {
    char c = path[i];

    if (escaped) {
      result += c;
      escaped = false;
      continue;
    }

    if (c == '\\' && !in_single_quote) {
      escaped = true;
      continue;
    }

    if (c == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
      continue;
    }

    if (c == '"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
      continue;
    }

    result += c;
  }

  return result;
}

// Helper function to quote a path if it contains spaces or special characters
std::string quote_path_if_needed(const std::string& path) {
  if (path.empty())
    return path;

  // Check if the path needs quoting
  bool needs_quoting = false;
  for (char c : path) {
    if (c == ' ' || c == '\t' || c == '\'' || c == '"' || c == '\\' ||
        c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
        c == '&' || c == '|' || c == ';' || c == '<' || c == '>' || c == '*' ||
        c == '?' || c == '$' || c == '`') {
      needs_quoting = true;
      break;
    }
  }

  if (!needs_quoting)
    return path;

  // Use double quotes and escape any double quotes or backslashes in the path
  std::string result = "\"";
  for (char c : path) {
    if (c == '"' || c == '\\') {
      result += '\\';
    }
    result += c;
  }
  result += "\"";

  return result;
}

CompletionContext detect_completion_context(const char* prefix) {
  std::string prefix_str(prefix);

  if (g_debug_mode)
    std::cerr << "DEBUG: Detecting completion context for prefix: '" << prefix
              << "'" << std::endl;

  // Check if it starts with a path indicator
  if (prefix_str.find('/') == 0 || prefix_str.find("./") == 0 ||
      prefix_str.find("../") == 0) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Context detected: PATH" << std::endl;
    return CONTEXT_PATH;
  }

  // Tokenize the command line to properly handle quotes and escapes
  std::vector<std::string> tokens = tokenize_command_line(prefix_str);

  if (tokens.size() > 1) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Context detected: ARGUMENT (found " << tokens.size()
                << " tokens)" << std::endl;
    return CONTEXT_ARGUMENT;
  }

  // Check if we have an unfinished token with spaces (indicating incomplete
  // argument)
  size_t last_unquoted_space = find_last_unquoted_space(prefix_str);
  if (last_unquoted_space != std::string::npos) {
    if (g_debug_mode)
      std::cerr
          << "DEBUG: Context detected: ARGUMENT (incomplete token with spaces)"
          << std::endl;
    return CONTEXT_ARGUMENT;
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: Context detected: COMMAND" << std::endl;
  return CONTEXT_COMMAND;
}

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Command completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv))
    return;

  std::string prefix_lower(prefix);
  std::transform(prefix_lower.begin(), prefix_lower.end(), prefix_lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  size_t prefix_len = prefix_lower.length();
  auto cmds = g_shell->get_available_commands();
  for (const auto& cmd : cmds) {
    std::string cmd_lower(cmd);
    std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (cmd_lower.rfind(prefix_lower, 0) == 0) {
      // Use full command name instead of just suffix
      long delete_before = static_cast<long>(prefix_len);
      if (g_debug_mode)
        std::cerr << "DEBUG: Command completion found: '" << cmd
                  << "' (deleting " << delete_before << " chars before)" << std::endl;
      
      // Use safe wrapper function
      if (!safe_add_completion_prim(cenv, cmd.c_str(), nullptr, nullptr, delete_before, 0))
        return;
    }
    if (ic_stop_completing(cenv))
      return;
  }

  if (g_debug_mode && !ic_has_completions(cenv))
    std::cerr << "DEBUG: No command completions found for prefix: '" << prefix
              << "'" << std::endl;
}

void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: History completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv))
    return;

  std::string prefix_str(prefix);
  std::string prefix_lower(prefix);
  std::transform(prefix_lower.begin(), prefix_lower.end(), prefix_lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  size_t prefix_len = prefix_str.length();  // Use original prefix length

  // Allow empty prefix for history completion
  if (prefix_len == 0) {
    if (g_debug_mode)
      std::cerr << "DEBUG: History completer with empty prefix (showing recent "
                   "history)"
                << std::endl;
  }

  std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path);
  if (!history_file.is_open()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Failed to open history file: "
                << cjsh_filesystem::g_cjsh_history_path << std::endl;
    return;
  }

  std::string line;
  std::vector<std::pair<std::string, int>> matches;

  while (std::getline(history_file, line)) {
    // Skip empty lines
    if (line.empty())
      continue;

    bool should_match = false;
    if (prefix_len == 0) {
      // For empty prefix, include all non-empty history entries
      should_match = (line != prefix_str);
    } else {
      // For non-empty prefix, do case-insensitive prefix matching
      std::string line_lower(line);
      std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      should_match =
          (line_lower.rfind(prefix_lower, 0) == 0 && line != prefix_str);
    }

    if (should_match) {
      if (g_completion_frequency.find(line) == g_completion_frequency.end()) {
        g_completion_frequency[line] = 1;
      }
      matches.push_back({line, g_completion_frequency[line]});
    }
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: Found " << matches.size()
              << " history matches for prefix: '" << prefix << "'" << std::endl;

  std::sort(matches.begin(), matches.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  const size_t max_suggestions = 20;
  size_t count = 0;

  for (const auto& match : matches) {
    // Always use the full command as completion text
    std::string completion = match.first;
    long delete_before = static_cast<long>(prefix_len);

    if (g_debug_mode)
      std::cerr << "DEBUG: Adding history completion: '" << match.first
                << "' -> '" << completion << "' (deleting " << delete_before 
                << " chars before, freq: " << match.second << ")" << std::endl;
    
    // Use safe wrapper function
    if (!safe_add_completion_prim(cenv, completion.c_str(), nullptr, nullptr, delete_before, 0))
      return;
    if (++count >= max_suggestions || ic_stop_completing(cenv))
      return;
  }
}

bool should_complete_directories_only(const std::string& prefix) {
  std::string command;
  size_t first_space = prefix.find(' ');

  if (first_space != std::string::npos) {
    command = prefix.substr(0, first_space);
  } else {
    return false;
  }

  static const std::unordered_set<std::string> directory_only_commands = {
      "cd", "ls", "dir", "rmdir"};

  return directory_only_commands.find(command) != directory_only_commands.end();
}

bool starts_with_case_insensitive(const std::string& str,
                                  const std::string& prefix) {
  if (prefix.length() > str.length()) {
    return false;
  }

  return std::equal(
      prefix.begin(), prefix.end(), str.begin(),
      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Filename completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv))
    return;

  std::string prefix_str(prefix);
  bool directories_only = should_complete_directories_only(prefix_str);

  if (g_debug_mode && directories_only)
    std::cerr << "DEBUG: Directory-only completion mode enabled for prefix: '"
              << prefix << "'" << std::endl;

  // Use the improved space finder that respects quotes and escapes
  size_t last_space = find_last_unquoted_space(prefix_str);

  bool has_tilde = false;
  bool has_dash = false;
  std::string prefix_before = "";
  std::string special_part = "";

  if (last_space != std::string::npos && last_space + 1 < prefix_str.length()) {
    if (prefix_str[last_space + 1] == '~') {
      has_tilde = true;
      prefix_before = prefix_str.substr(0, last_space + 1);
      special_part = prefix_str.substr(last_space + 1);
    } else if (prefix_str[last_space + 1] == '-' &&
               (prefix_str.length() == last_space + 2 ||
                prefix_str[last_space + 2] == '/')) {
      has_dash = true;
      prefix_before = prefix_str.substr(0, last_space + 1);
      special_part = prefix_str.substr(last_space + 1);
    } else {
      prefix_before = prefix_str.substr(0, last_space + 1);
      special_part = prefix_str.substr(last_space + 1);
    }
  } else if (prefix_str[0] == '~') {
    has_tilde = true;
    special_part = prefix_str;
  } else if (prefix_str[0] == '-' &&
             (prefix_str.length() == 1 || prefix_str[1] == '/')) {
    has_dash = true;
    special_part = prefix_str;
  } else if (prefix_str.rfind("cd ", 0) == 0 && prefix_str.length() > 3) {
    prefix_before = "cd ";
    special_part = prefix_str.substr(3);
  }

  if (has_tilde && (special_part.length() == 1 || special_part[1] == '/')) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing tilde completion: '" << special_part
                << "'" << std::endl;

    std::string unquoted_special = unquote_path(special_part);
    std::string path_after_tilde =
        unquoted_special.length() > 1 ? unquoted_special.substr(2) : "";
    std::string dir_to_complete = cjsh_filesystem::g_user_home_path.string();

    if (unquoted_special.length() > 1) {
      dir_to_complete += "/" + path_after_tilde;
    }

    namespace fs = std::filesystem;
    fs::path dir_path;
    std::string match_prefix;

    if (unquoted_special.back() == '/') {
      dir_path = dir_to_complete;
      match_prefix = "";
    } else {
      size_t last_slash = dir_to_complete.find_last_of('/');
      if (last_slash != std::string::npos) {
        dir_path = dir_to_complete.substr(0, last_slash);
        match_prefix = dir_to_complete.substr(last_slash + 1);
      } else {
        dir_path = dir_to_complete;
        match_prefix = "";
      }
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: Looking in directory: '" << dir_path << "'"
                << std::endl;
      std::cerr << "DEBUG: Matching prefix: '" << match_prefix << "'"
                << std::endl;
    }

    try {
      if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          std::string filename = entry.path().filename().string();

          if (match_prefix.empty() ||
              starts_with_case_insensitive(filename, match_prefix)) {
            std::string completion_suffix;

            if (match_prefix.empty()) {
              completion_suffix = quote_path_if_needed(filename);
              if (entry.is_directory()) {
                completion_suffix += "/";
              }

              if (g_debug_mode)
                std::cerr << "DEBUG: Adding tilde completion: '"
                          << completion_suffix << "'" << std::endl;

              if (!safe_add_completion(cenv, completion_suffix.c_str()))
                return;
            } else {
              // Use full filename and calculate how many characters to delete
              completion_suffix = quote_path_if_needed(filename);
              if (entry.is_directory()) {
                completion_suffix += "/";
              }
              long delete_before = static_cast<long>(match_prefix.length());

              if (g_debug_mode)
                std::cerr
                    << "DEBUG: Adding tilde completion (full name): '"
                    << completion_suffix << "' (deleting " << delete_before
                    << " chars before)" << std::endl;

              if (!safe_add_completion_prim(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, delete_before, 0))
                return;
            }
          }
        }
      }
    } catch (const std::exception& e) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Error reading directory: " << e.what()
                  << std::endl;
    }

    return;
  }

  else if (has_dash && (special_part.length() == 1 || special_part[1] == '/')) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Processing dash completion for previous directory: '"
                << special_part << "'" << std::endl;

    std::string unquoted_special = unquote_path(special_part);
    std::string path_after_dash =
        unquoted_special.length() > 1 ? unquoted_special.substr(2) : "";
    std::string dir_to_complete = g_shell->get_previous_directory();

    if (dir_to_complete.empty()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: No previous directory set" << std::endl;
      return;
    }

    if (unquoted_special.length() > 1) {
      dir_to_complete += "/" + path_after_dash;
    }

    namespace fs = std::filesystem;
    fs::path dir_path;
    std::string match_prefix;

    if (unquoted_special.back() == '/') {
      dir_path = dir_to_complete;
      match_prefix = "";
    } else {
      size_t last_slash = dir_to_complete.find_last_of('/');
      if (last_slash != std::string::npos) {
        dir_path = dir_to_complete.substr(0, last_slash);
        match_prefix = dir_to_complete.substr(last_slash + 1);
      } else {
        dir_path = dir_to_complete;
        match_prefix = "";
      }
    }

    if (g_debug_mode) {
      std::cerr << "DEBUG: Looking in directory: '" << dir_path << "'"
                << std::endl;
      std::cerr << "DEBUG: Matching prefix: '" << match_prefix << "'"
                << std::endl;
    }

    try {
      if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          std::string filename = entry.path().filename().string();

          if (match_prefix.empty() ||
              starts_with_case_insensitive(filename, match_prefix)) {
            std::string completion_suffix;

            if (match_prefix.empty()) {
              completion_suffix = quote_path_if_needed(filename);
              if (entry.is_directory()) {
                completion_suffix += "/";
              }

              if (g_debug_mode)
                std::cerr << "DEBUG: Adding dash completion: '"
                          << completion_suffix << "'" << std::endl;

              if (!safe_add_completion(cenv, completion_suffix.c_str()))
                return;
            } else {
              // Use full filename and calculate how many characters to delete
              completion_suffix = quote_path_if_needed(filename);
              if (entry.is_directory()) {
                completion_suffix += "/";
              }
              long delete_before = static_cast<long>(match_prefix.length());

              if (g_debug_mode)
                std::cerr << "DEBUG: Adding dash completion (full name): '"
                          << completion_suffix << "' (deleting "
                          << delete_before << " chars before)" << std::endl;

              if (!safe_add_completion_prim(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, delete_before, 0))
                return;
            }
          }
        }
      }
    } catch (const std::exception& e) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Error reading directory: " << e.what()
                  << std::endl;
    }

    return;
  }

  if (!prefix_before.empty()) {
    std::string command_part = prefix_before;

    while (!command_part.empty() &&
           (command_part.back() == ' ' || command_part.back() == '\t')) {
      command_part.pop_back();
    }

    if (command_part == "cd" || command_part.rfind("cd ", 0) == 0) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Processing bookmark completions for cd command "
                     "with prefix: '"
                  << special_part << "'" << std::endl;

      if (g_shell && g_shell->get_built_ins()) {
        const auto& bookmarks =
            g_shell->get_built_ins()->get_directory_bookmarks();

        for (const auto& bookmark : bookmarks) {
          const std::string& bookmark_name = bookmark.first;
          const std::string& bookmark_path = bookmark.second;

          if (special_part.empty() ||
              bookmark_name.rfind(special_part, 0) == 0) {
            
            // For cd commands, check if the bookmark is a directory
            namespace fs = std::filesystem;
            if (fs::exists(bookmark_path) && fs::is_directory(bookmark_path)) {
              // Calculate how many characters to delete before inserting the completion
              size_t delete_before = special_part.length();
              // Don't add trailing slash for bookmarks - let user add it if they want
              std::string completion_text = bookmark_name;
              
              if (g_debug_mode)
                std::cerr << "DEBUG: Adding bookmark completion: '"
                          << bookmark_name << "' -> '" << completion_text << "' (deleting " << delete_before << " chars before)"
                          << std::endl;

              if (!safe_add_completion_prim(cenv, completion_text.c_str(), NULL, NULL, delete_before, 0))
                return;
            }
          }
        }
      }
    }
  }

  std::string path_to_check = special_part.empty() ? unquote_path(prefix_str)
                                                   : unquote_path(special_part);

  if (!ic_stop_completing(cenv) && !path_to_check.empty() &&
      path_to_check.back() == '/') {
    namespace fs = std::filesystem;
    fs::path dir_path(path_to_check);
    try {
      if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
        for (auto& entry : fs::directory_iterator(dir_path)) {
          std::string name = entry.path().filename().string();

          if (directories_only && !entry.is_directory()) {
            continue;
          }

          std::string suffix = quote_path_if_needed(name);
          if (entry.is_directory()) {
            suffix += "/";
          }
          if (g_debug_mode)
            std::cerr << "DEBUG: All files completion: '" << suffix << "'"
                      << std::endl;
          if (!safe_add_completion(cenv, suffix.c_str()))
            return;
          if (ic_stop_completing(cenv))
            return;
        }
      }
    } catch (const std::exception& e) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Error reading directory for all files completion: "
                  << e.what() << std::endl;
    }
    return;
  }

  if (directories_only) {
    std::string path_to_complete = special_part.empty()
                                       ? unquote_path(prefix_str)
                                       : unquote_path(special_part);

    namespace fs = std::filesystem;
    fs::path dir_path;
    std::string match_prefix;

    if (path_to_complete.empty() || path_to_complete.back() == '/') {
      dir_path = path_to_complete.empty() ? "." : path_to_complete;
      match_prefix = "";
    } else {
      size_t last_slash = path_to_complete.find_last_of('/');
      if (last_slash != std::string::npos) {
        dir_path = path_to_complete.substr(0, last_slash);
        if (dir_path.empty())
          dir_path = "/";
        match_prefix = path_to_complete.substr(last_slash + 1);
      } else {
        dir_path = ".";
        match_prefix = path_to_complete;
      }
    }

    try {
      if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          if (!entry.is_directory())
            continue;

          std::string filename = entry.path().filename().string();

          if (filename[0] == '.' && match_prefix.empty())
            continue;

          if (match_prefix.empty() ||
              starts_with_case_insensitive(filename, match_prefix)) {
            std::string completion_suffix;

            if (match_prefix.empty()) {
              completion_suffix = quote_path_if_needed(filename);
              completion_suffix += "/";

              if (g_debug_mode)
                std::cerr << "DEBUG: Directory-only completion: '"
                          << completion_suffix << "'" << std::endl;

              if (!safe_add_completion(cenv, completion_suffix.c_str()))
                return;
            } else {
              // Use full filename and calculate how many characters to delete
              completion_suffix = quote_path_if_needed(filename);
              completion_suffix += "/";
              long delete_before = static_cast<long>(match_prefix.length());

              if (g_debug_mode)
                std::cerr
                    << "DEBUG: Directory-only completion (full name): '"
                    << completion_suffix << "' (deleting " << delete_before
                    << " chars before)" << std::endl;

              if (!safe_add_completion_prim(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, delete_before, 0))
                return;
            }
          }

          if (ic_stop_completing(cenv))
            return;
        }
      }
    } catch (const std::exception& e) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Error in directory-only completion: " << e.what()
                  << std::endl;
    }
  } else {
    // Handle general filename completion with proper quote/space handling
    std::string path_to_complete;
    std::string prefix_for_completion;

    if (!special_part.empty()) {
      path_to_complete = unquote_path(special_part);
      prefix_for_completion = special_part;
    } else {
      path_to_complete = unquote_path(prefix_str);
      prefix_for_completion = prefix_str;
    }

    if (g_debug_mode)
      std::cerr << "DEBUG: General filename completion for unquoted path: '"
                << path_to_complete << "'" << std::endl;

    namespace fs = std::filesystem;
    fs::path dir_path;
    std::string match_prefix;

    if (path_to_complete.empty() || path_to_complete.back() == '/') {
      dir_path = path_to_complete.empty() ? "." : path_to_complete;
      match_prefix = "";
    } else {
      size_t last_slash = path_to_complete.find_last_of('/');
      if (last_slash != std::string::npos) {
        dir_path = path_to_complete.substr(0, last_slash);
        if (dir_path.empty())
          dir_path = "/";
        match_prefix = path_to_complete.substr(last_slash + 1);
      } else {
        dir_path = ".";
        match_prefix = path_to_complete;
      }
    }

    try {
      if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          std::string filename = entry.path().filename().string();

          // Skip hidden files if match_prefix doesn't start with dot
          if (filename[0] == '.' && match_prefix.empty())
            continue;

          if (match_prefix.empty() ||
              starts_with_case_insensitive(filename, match_prefix)) {
            std::string completion_suffix;
            if (match_prefix.empty()) {
              completion_suffix = quote_path_if_needed(filename);
              if (entry.is_directory()) {
                completion_suffix += "/";
              }

              if (g_debug_mode)
                std::cerr << "DEBUG: General filename completion: '"
                          << completion_suffix << "'" << std::endl;

              if (!safe_add_completion(cenv, completion_suffix.c_str()))
                return;
            } else {
              // Use full filename and calculate how many characters to delete
              completion_suffix = quote_path_if_needed(filename);
              if (entry.is_directory()) {
                completion_suffix += "/";
              }

              long delete_before = static_cast<long>(match_prefix.length());

              if (g_debug_mode)
                std::cerr
                    << "DEBUG: General filename completion (full name): '"
                    << completion_suffix << "' (deleting " << delete_before
                    << " chars before)" << std::endl;

              if (!safe_add_completion_prim(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, delete_before, 0))
                return;
            }
          }

          if (ic_stop_completing(cenv))
            return;
        }
      }
    } catch (const std::exception& e) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Error in general filename completion: " << e.what()
                  << std::endl;
    }
  }

  if (g_debug_mode) {
    if (ic_has_completions(cenv))
      std::cerr << "DEBUG: Filename completions found for prefix: '" << prefix
                << "'" << std::endl;
    else
      std::cerr << "DEBUG: No filename completions found for prefix: '"
                << prefix << "'" << std::endl;
  }
}

void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: Default completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv))
    return;
  
  // Set up completion deduplication tracker
  CompletionTracker tracker(cenv, prefix);
  g_current_completion_tracker = &tracker;
  
  CompletionContext context = detect_completion_context(prefix);

  switch (context) {
    case CONTEXT_COMMAND:
      cjsh_history_completer(cenv, prefix);
      if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
        g_current_completion_tracker = nullptr;
        return;
      }

      cjsh_command_completer(cenv, prefix);
      if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
        g_current_completion_tracker = nullptr;
        return;
      }

      cjsh_filename_completer(cenv, prefix);
      break;

    case CONTEXT_PATH:
      cjsh_history_completer(cenv, prefix);
      cjsh_filename_completer(cenv, prefix);
      break;

    case CONTEXT_ARGUMENT: {
      // Check if this is a cd command - if so, only use filename completion
      std::string prefix_str(prefix);
      std::vector<std::string> tokens = tokenize_command_line(prefix_str);
      
      if (!tokens.empty() && tokens[0] == "cd") {
        if (g_debug_mode)
          std::cerr << "DEBUG: Detected cd command, using only filename completion" << std::endl;
        cjsh_filename_completer(cenv, prefix);
      } else {
        cjsh_history_completer(cenv, prefix);
        cjsh_filename_completer(cenv, prefix);
      }
      break;
    }
  }
  
  // Clean up tracker
  g_current_completion_tracker = nullptr;
}

void initialize_completion_system() {
  if (g_debug_mode)
    std::cerr << "DEBUG: Initializing completion system" << std::endl;
  ic_style_def("cjsh-unknown-command", "bold color=#FF5555");
  ic_style_def("cjsh-colon", "bold color=#8BE9FD");
  ic_style_def("cjsh-path-exists", "color=#50FA7B");
  ic_style_def("cjsh-path-not-exists", "color=#FF5555");
  ic_style_def("cjsh-glob-pattern", "color=#F1FA8C");
  ic_style_def("cjsh-operator", "bold color=#FF79C6");
  ic_style_def("cjsh-keyword", "bold color=#BD93F9");

  ic_style_def("cjsh-builtin", "color=#FFB86C");
  ic_style_def("cjsh-variable", "color=#8BE9FD");
  ic_style_def("cjsh-string", "color=#F1FA8C");
  ic_style_def("cjsh-comment", "color=#6272A4");
  ic_style_def("cjsh-known-command", "color=#50FA7B");
  ic_style_def("cjsh-external-command", "color=#8BE9FD");
  ic_style_def("cjsh-function-definition", "bold color=#F1FA8C");

  if (config::completions_enabled) {
    ic_set_default_completer(cjsh_default_completer, NULL);
    ic_enable_completion_preview(true);
    ic_enable_hint(true);
    ic_set_hint_delay(0);
    ic_enable_auto_tab(false);
    ic_enable_completion_preview(true);
  } else {
    ic_set_default_completer(nullptr, NULL);
    ic_enable_completion_preview(false);
    ic_enable_hint(false);
    ic_enable_auto_tab(false);
  }

  if (config::syntax_highlighting_enabled) {
    SyntaxHighlighter::initialize();
    ic_set_default_highlighter(SyntaxHighlighter::highlight, NULL);
    ic_enable_highlight(true);
  } else {
    ic_set_default_highlighter(nullptr, NULL);
    ic_enable_highlight(false);
  }

  ic_enable_history_duplicates(false);
  ic_enable_inline_help(false);
  ic_enable_multiline_indent(true);
  ic_enable_multiline(true);
  ic_set_prompt_marker("", NULL);
  ic_set_history(cjsh_filesystem::g_cjsh_history_path.c_str(), -1);
}

void update_completion_frequency(const std::string& command) {
  if (g_debug_mode) {
    if (!command.empty())
      std::cerr << "DEBUG: Updating completion frequency for command: '"
                << command << "'" << std::endl;
    else
      std::cerr << "DEBUG: Skipped updating frequency (empty command)"
                << std::endl;
  }

  if (!command.empty()) {
    g_completion_frequency[command]++;
  }
}
