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
static const size_t MAX_COMPLETION_TRACKER_ENTRIES = 500;
static const size_t MAX_TOTAL_COMPLETIONS = 100;

enum CompletionContext {
  CONTEXT_COMMAND,
  CONTEXT_ARGUMENT,
  CONTEXT_PATH
};

// Source priority levels (higher number = higher priority)
enum SourcePriority {
  PRIORITY_HISTORY = 0,
  PRIORITY_BOOKMARK = 1,
  PRIORITY_UNKNOWN = 2,
  PRIORITY_FILE = 3,
  PRIORITY_DIRECTORY = 4,
  PRIORITY_PLUGIN = 5,
  PRIORITY_FUNCTION = 6
};

// Helper function to get priority for a source string
static SourcePriority get_source_priority(const char* source) {
  if (!source) return PRIORITY_UNKNOWN;
  
  if (strcmp(source, "history") == 0) return PRIORITY_HISTORY;
  if (strcmp(source, "bookmark") == 0) return PRIORITY_BOOKMARK;
  if (strcmp(source, "file") == 0) return PRIORITY_FILE;
  if (strcmp(source, "directory") == 0) return PRIORITY_DIRECTORY;
  if (strcmp(source, "plugin") == 0) return PRIORITY_PLUGIN;
  if (strcmp(source, "function") == 0) return PRIORITY_FUNCTION;
  
  return PRIORITY_UNKNOWN;
}

// Completion deduplication system with source priority
struct CompletionTracker {
  std::unordered_map<std::string, SourcePriority> added_completions;
  ic_completion_env_t* cenv;
  std::string original_prefix;
  size_t total_completions_added;
  
  CompletionTracker(ic_completion_env_t* env, const char* prefix) : cenv(env), original_prefix(prefix), total_completions_added(0) {
    // Reserve some space to avoid frequent reallocations
    added_completions.reserve(128);
  }
  
  ~CompletionTracker() {
    // Explicit cleanup
    if (g_debug_mode && added_completions.size() > 1000) {
      std::cerr << "DEBUG: CompletionTracker had large size: " << added_completions.size() << std::endl;
    }
    added_completions.clear();
  }
  
  // Check if we've reached the global completion limit
  bool has_reached_completion_limit() const {
    return total_completions_added >= MAX_TOTAL_COMPLETIONS;
  }
  
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
  // Returns true if we should skip this completion (lower or equal priority exists)
  bool would_create_duplicate(const char* completion_text, const char* source, long delete_before = 0) {
    // Prevent unbounded growth
    if (added_completions.size() >= MAX_COMPLETION_TRACKER_ENTRIES) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: CompletionTracker reached max entries (" << MAX_COMPLETION_TRACKER_ENTRIES 
                  << "), skipping further additions" << std::endl;
      }
      return true; // Skip to prevent memory explosion
    }
    
    std::string final_result = calculate_final_result(completion_text, delete_before);
    auto it = added_completions.find(final_result);
    
    if (it == added_completions.end()) {
      // Check for bookmark vs directory conflicts
      if (source && strcmp(source, "bookmark") == 0) {
        // Check if there's already a directory with the same name (final_result + "/")
        std::string directory_result = final_result + "/";
        auto dir_it = added_completions.find(directory_result);
        if (dir_it != added_completions.end()) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: Skipping bookmark '" << completion_text 
                      << "' because directory with same name exists" << std::endl;
          }
          return true; // Skip bookmark because directory exists
        }
      } else if (source && strcmp(source, "directory") == 0) {
        // Check if there's already a bookmark with the same base name
        std::string bookmark_result = final_result;
        if (bookmark_result.back() == '/') {
          bookmark_result.pop_back(); // Remove trailing slash
          auto bookmark_it = added_completions.find(bookmark_result);
          if (bookmark_it != added_completions.end() && 
              bookmark_it->second == PRIORITY_BOOKMARK) {
            if (g_debug_mode) {
              std::cerr << "DEBUG: Directory '" << completion_text 
                        << "' will replace existing bookmark with same name" << std::endl;
            }
            // Remove the bookmark entry since directory has higher priority
            added_completions.erase(bookmark_it);
          }
        }
      }
      // No duplicate, safe to add
      return false;
    }
    
    // Duplicate exists, check priority
    SourcePriority existing_priority = it->second;
    SourcePriority new_priority = get_source_priority(source);
    
    // If new completion has higher priority, we'll replace the old one
    return new_priority <= existing_priority;
  }
  
  // Wrapper for ic_add_completion that checks for duplicates
  bool add_completion_if_unique(const char* completion_text) {
    const char* source = nullptr; // Default source for backward compatibility
    if (has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached global completion limit (" << MAX_TOTAL_COMPLETIONS << "), skipping: '" << completion_text << "'" << std::endl;
      return true; // Don't fail, just skip
    }
    if (would_create_duplicate(completion_text, source, 0)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Skipping duplicate completion: '" << completion_text << "'" << std::endl;
      return true; // Don't fail, just skip the duplicate
    }
    
    std::string final_result = calculate_final_result(completion_text, 0);
    added_completions[final_result] = get_source_priority(source);
    total_completions_added++;
    if (g_debug_mode)
      std::cerr << "DEBUG: Adding unique completion: '" << completion_text << "' -> final: '" << final_result << "' (total: " << total_completions_added << ")" << std::endl;
    return ic_add_completion_ex_with_source(cenv, completion_text, nullptr, nullptr, source);
  }
  
  // Wrapper for ic_add_completion_prim that checks for duplicates
  bool add_completion_prim_if_unique(const char* completion_text, const char* display, const char* help, long delete_before, long delete_after) {
    const char* source = nullptr; // Default source for backward compatibility
    if (has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached global completion limit (" << MAX_TOTAL_COMPLETIONS << "), skipping (prim): '" << completion_text << "'" << std::endl;
      return true; // Don't fail, just skip
    }
    if (would_create_duplicate(completion_text, source, delete_before)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Skipping duplicate completion (prim): '" << completion_text << "'" << std::endl;
      return true; // Don't fail, just skip the duplicate
    }
    
    std::string final_result = calculate_final_result(completion_text, delete_before);
    added_completions[final_result] = get_source_priority(source);
    total_completions_added++;
    if (g_debug_mode)
      std::cerr << "DEBUG: Adding unique completion (prim): '" << completion_text << "' -> final: '" << final_result << "' (total: " << total_completions_added << ")" << std::endl;
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source, delete_before, delete_after);
  }
  
  // Wrapper for ic_add_completion_prim_with_source that checks for duplicates
  bool add_completion_prim_with_source_if_unique(const char* completion_text, const char* display, const char* help, const char* source, long delete_before, long delete_after) {
    if (has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached global completion limit (" << MAX_TOTAL_COMPLETIONS << "), skipping (prim with source): '" << completion_text << "'" << std::endl;
      return true; // Don't fail, just skip
    }
    
    std::string final_result = calculate_final_result(completion_text, delete_before);
    auto it = added_completions.find(final_result);
    SourcePriority new_priority = get_source_priority(source);
    
    // Check for bookmark vs directory conflicts
    if (it == added_completions.end()) {
      if (source && strcmp(source, "bookmark") == 0) {
        // Check if there's already a directory with the same name (final_result + "/")
        std::string directory_result = final_result + "/";
        auto dir_it = added_completions.find(directory_result);
        if (dir_it != added_completions.end()) {
          if (g_debug_mode) {
            std::cerr << "DEBUG: Skipping bookmark '" << completion_text 
                      << "' because directory with same name exists" << std::endl;
          }
          return true; // Skip bookmark because directory exists
        }
      } else if (source && strcmp(source, "directory") == 0) {
        // Check if there's already a bookmark with the same base name
        std::string bookmark_result = final_result;
        if (bookmark_result.back() == '/') {
          bookmark_result.pop_back(); // Remove trailing slash
          auto bookmark_it = added_completions.find(bookmark_result);
          if (bookmark_it != added_completions.end() && 
              bookmark_it->second == PRIORITY_BOOKMARK) {
            if (g_debug_mode) {
              std::cerr << "DEBUG: Directory '" << completion_text 
                        << "' will replace existing bookmark with same name" << std::endl;
            }
            // Remove the bookmark entry since directory has higher priority
            added_completions.erase(bookmark_it);
          }
        }
      }
      // New completion, increment counter
      total_completions_added++;
    } else {
      SourcePriority existing_priority = it->second;
      
      if (new_priority <= existing_priority) {
        // Lower or equal priority, skip this completion
        if (g_debug_mode)
          std::cerr << "DEBUG: Skipping lower/equal priority completion (prim with source): '" 
                    << completion_text << "' (source: '" << (source ? source : "null") 
                    << "', priority: " << new_priority << " vs existing: " << existing_priority << ")" << std::endl;
        return true; // Don't fail, just skip
      }
      
      // Higher priority completion, we'll replace the existing one
      if (g_debug_mode)
        std::cerr << "DEBUG: Replacing lower priority completion with: '" 
                  << completion_text << "' (source: '" << (source ? source : "null") 
                  << "', priority: " << new_priority << " vs existing: " << existing_priority << ")" << std::endl;
    }
    
    // Update or add the completion with new priority
    added_completions[final_result] = new_priority;
    if (g_debug_mode)
      std::cerr << "DEBUG: Adding unique completion (prim with source): '" << completion_text 
                << "' (source: '" << (source ? source : "null") << "') -> final: '" << final_result << "' (total: " << total_completions_added << ")" << std::endl;
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source, delete_before, delete_after);
  }
};

// Thread-local completion tracker for the current completion session
thread_local CompletionTracker* g_current_completion_tracker = nullptr;

// RAII wrapper for completion tracker to ensure cleanup
class CompletionSession {
public:
  CompletionSession(ic_completion_env_t* cenv, const char* prefix) {
    if (g_current_completion_tracker) {
      if (g_debug_mode) {
        std::cerr << "DEBUG: Warning - completion tracker already exists, cleaning up previous session" << std::endl;
      }
      delete g_current_completion_tracker;
    }
    
    g_current_completion_tracker = new CompletionTracker(cenv, prefix);
  }
  
  ~CompletionSession() {
    if (g_current_completion_tracker) {
      delete g_current_completion_tracker;
      g_current_completion_tracker = nullptr;
    }
  }
  
  // Prevent copying
  CompletionSession(const CompletionSession&) = delete;
  CompletionSession& operator=(const CompletionSession&) = delete;
};

// Helper functions that automatically use deduplication when available
bool safe_add_completion(ic_completion_env_t* cenv, const char* completion_text) {
  if (g_current_completion_tracker) {
    return g_current_completion_tracker->add_completion_if_unique(completion_text);
  } else {
    return ic_add_completion_ex_with_source(cenv, completion_text, nullptr, nullptr, nullptr);
  }
}

bool safe_add_completion_with_source(ic_completion_env_t* cenv, const char* completion_text, const char* source) {
  if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Reached global completion limit, skipping: '" << completion_text << "'" << std::endl;
    return true; // Don't fail, just skip
  }
  return ic_add_completion_ex_with_source(cenv, completion_text, nullptr, nullptr, source);
}

bool safe_add_completion_prim(ic_completion_env_t* cenv, const char* completion_text, const char* display, const char* help, long delete_before, long delete_after) {
  if (g_current_completion_tracker) {
    return g_current_completion_tracker->add_completion_prim_if_unique(completion_text, display, help, delete_before, delete_after);
  } else {
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, nullptr, delete_before, delete_after);
  }
}

bool safe_add_completion_prim_with_source(ic_completion_env_t* cenv, const char* completion_text, const char* display, const char* help, const char* source, long delete_before, long delete_after) {
  if (g_current_completion_tracker) {
    return g_current_completion_tracker->add_completion_prim_with_source_if_unique(completion_text, display, help, source, delete_before, delete_after);
  } else {
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source, delete_before, delete_after);
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

// Helper function to check if a builtin command should be included in interactive completions
bool is_interactive_builtin(const std::string& cmd) {
  // Commands that are primarily for shell script interpreter and shouldn't appear in interactive completions
  static const std::unordered_set<std::string> script_only_builtins = {
    "break",              // Loop control - only meaningful in scripts/functions
    "continue",           // Loop control - only meaningful in scripts/functions  
    "return",             // Function control - only meaningful in functions
    "__INTERNAL_SUBSHELL__", // Internal command - not for user interaction
    "local",              // Variable scoping - primarily for functions
    "shift",              // Argument shifting - primarily for scripts/functions
    "if",                 // Control flow syntax - part of larger constructs
    "[[",                 // Test syntax - part of conditional constructs
    "[",                  // Test syntax - part of conditional constructs
    ":",                  // No-op command - rarely used interactively
    "login-startup-arg",  // Startup configuration - not for interactive use
    "prompt_test"         // Development/testing command
  };
  
  return script_only_builtins.find(cmd) == script_only_builtins.end();
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
    
  // Check global completion limit
  if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Global completion limit reached, skipping command completer" << std::endl;
    return;
  }

  std::string prefix_lower(prefix);
  std::transform(prefix_lower.begin(), prefix_lower.end(), prefix_lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  size_t prefix_len = prefix_lower.length();
  
  // Get different types of commands
  std::vector<std::string> builtin_cmds;
  std::vector<std::string> function_names;
  std::vector<std::string> plugin_cmds;
  std::unordered_set<std::string> aliases;
  std::vector<std::filesystem::path> cached_executables;
  
  if (g_shell && g_shell->get_built_ins()) {
    builtin_cmds = g_shell->get_built_ins()->get_builtin_commands();
  }
  
  // Get user-defined functions from the script interpreter
  if (g_shell && g_shell->get_shell_script_interpreter()) {
    function_names = g_shell->get_shell_script_interpreter()->get_function_names();
  }
  
  // Get plugin commands
  if (g_plugin) {
    auto enabled_plugins = g_plugin->get_enabled_plugins();
    for (const auto& plugin : enabled_plugins) {
      auto plugin_commands = g_plugin->get_plugin_commands(plugin);
      plugin_cmds.insert(plugin_cmds.end(), plugin_commands.begin(), plugin_commands.end());
    }
  }
  
  // Get aliases
  if (g_shell) {
    auto shell_aliases = g_shell->get_aliases();
    for (const auto& alias : shell_aliases) {
      aliases.insert(alias.first);
    }
  }
  
  // Get cached executables
  cached_executables = cjsh_filesystem::read_cached_executables();

  // Add builtin commands first (filtered for interactive use)
  for (const auto& cmd : builtin_cmds) {
    if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached completion limit in builtin commands" << std::endl;
      return;
    }
    
    // Skip script-only builtins in interactive completions
    if (!is_interactive_builtin(cmd)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Skipping script-only builtin: '" << cmd << "'" << std::endl;
      continue;
    }
    
    std::string cmd_lower(cmd);
    std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (cmd_lower.rfind(prefix_lower, 0) == 0) {
      long delete_before = static_cast<long>(prefix_len);
      if (g_debug_mode)
        std::cerr << "DEBUG: Builtin command completion found: '" << cmd
                  << "' (deleting " << delete_before << " chars before)" << std::endl;
      
      if (!safe_add_completion_prim_with_source(cenv, cmd.c_str(), nullptr, nullptr, "builtin", delete_before, 0))
        return;
    }
    if (ic_stop_completing(cenv))
      return;
  }
  
  // Add user-defined functions with "function" source
  for (const auto& cmd : function_names) {
    if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached completion limit in function commands" << std::endl;
      return;
    }
    std::string cmd_lower(cmd);
    std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (cmd_lower.rfind(prefix_lower, 0) == 0) {
      long delete_before = static_cast<long>(prefix_len);
      if (g_debug_mode)
        std::cerr << "DEBUG: Function completion found: '" << cmd
                  << "' (deleting " << delete_before << " chars before)" << std::endl;
      
      if (!safe_add_completion_prim_with_source(cenv, cmd.c_str(), nullptr, nullptr, "function", delete_before, 0))
        return;
    }
    if (ic_stop_completing(cenv))
      return;
  }
  
  // Add plugin commands with "plugin" source
  for (const auto& cmd : plugin_cmds) {
    if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached completion limit in plugin commands" << std::endl;
      return;
    }
    std::string cmd_lower(cmd);
    std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (cmd_lower.rfind(prefix_lower, 0) == 0) {
      long delete_before = static_cast<long>(prefix_len);
      if (g_debug_mode)
        std::cerr << "DEBUG: Plugin command completion found: '" << cmd
                  << "' (deleting " << delete_before << " chars before)" << std::endl;
      
      if (!safe_add_completion_prim_with_source(cenv, cmd.c_str(), nullptr, nullptr, "plugin", delete_before, 0))
        return;
    }
    if (ic_stop_completing(cenv))
      return;
  }
  
  // Add aliases with "alias" source
  for (const auto& cmd : aliases) {
    if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached completion limit in aliases" << std::endl;
      return;
    }
    std::string cmd_lower(cmd);
    std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (cmd_lower.rfind(prefix_lower, 0) == 0) {
      long delete_before = static_cast<long>(prefix_len);
      if (g_debug_mode)
        std::cerr << "DEBUG: Alias completion found: '" << cmd
                  << "' (deleting " << delete_before << " chars before)" << std::endl;
      
      if (!safe_add_completion_prim_with_source(cenv, cmd.c_str(), nullptr, nullptr, "alias", delete_before, 0))
        return;
    }
    if (ic_stop_completing(cenv))
      return;
  }
  
  // Add cached executables with "system" source
  for (const auto& exec_path : cached_executables) {
    if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached completion limit in cached executables" << std::endl;
      return;
    }
    std::string cmd = exec_path.filename().string();
    std::string cmd_lower(cmd);
    std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (cmd_lower.rfind(prefix_lower, 0) == 0) {
      long delete_before = static_cast<long>(prefix_len);
      if (g_debug_mode)
        std::cerr << "DEBUG: Executable completion found: '" << cmd
                  << "' (deleting " << delete_before << " chars before)" << std::endl;
      
      if (!safe_add_completion_prim_with_source(cenv, cmd.c_str(), nullptr, nullptr, "system", delete_before, 0))
        return;
    }
    if (ic_stop_completing(cenv))
      return;
  }

  if (g_debug_mode && !ic_has_completions(cenv))
    std::cerr << "DEBUG: No command completions found for prefix: '" << prefix
              << "'" << std::endl;
}

// Helper function to check if a string looks like a file path
bool looks_like_file_path(const std::string& str) {
  if (str.empty()) return false;
  
  // Check for common path indicators
  if (str[0] == '/' ||                          // Absolute path
      str.rfind("./", 0) == 0 ||                // Relative path starting with ./
      str.rfind("../", 0) == 0 ||               // Relative path starting with ../
      str.rfind("~/", 0) == 0 ||                // Home directory path
      str.find('/') != std::string::npos) {     // Contains path separator
    return true;
  }
  
  // Check if it looks like a filename with common extensions
  size_t dot_pos = str.rfind('.');
  if (dot_pos != std::string::npos && dot_pos > 0 && dot_pos < str.length() - 1) {
    std::string extension = str.substr(dot_pos + 1);
    // Common file extensions
    static const std::unordered_set<std::string> file_extensions = {
      "txt", "log", "conf", "config", "json", "xml", "yaml", "yml",
      "cpp", "c", "h", "hpp", "py", "js", "ts", "java", "sh", "bash",
      "md", "html", "css", "sql", "tar", "gz", "zip", "pdf", "doc",
      "docx", "xls", "xlsx", "png", "jpg", "jpeg", "gif", "mp3", "mp4"
    };
    
    std::string ext_lower = extension;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (file_extensions.find(ext_lower) != file_extensions.end()) {
      return true;
    }
  }
  
  return false;
}

void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix) {
  if (g_debug_mode)
    std::cerr << "DEBUG: History completer called with prefix: '" << prefix
              << "'" << std::endl;

  if (ic_stop_completing(cenv))
    return;
    
  // Check global completion limit
  if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Global completion limit reached, skipping history completer" << std::endl;
    return;
  }

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

  // Use vector with reserved space to avoid frequent reallocations
  std::vector<std::pair<std::string, int>> matches;
  matches.reserve(50); // Reduce from 100 to 50 to save memory
  
  std::string line;
  line.reserve(256); // Reserve space for typical command line length

  while (std::getline(history_file, line) && matches.size() < 50) { // Reduce limit from 200 to 50
    // Skip empty lines
    if (line.empty())
      continue;

    // Skip timestamp lines (format: "# <timestamp>")
    if (line.length() > 1 && line[0] == '#' && line[1] == ' ') {
      if (g_debug_mode)
        std::cerr << "DEBUG: Skipping timestamp line: '" << line << "'" << std::endl;
      continue;
    }

    // Skip entries that look like file paths
    if (looks_like_file_path(line)) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Skipping path-like history entry: '" << line << "'" << std::endl;
      continue;
    }

    bool should_match = false;
    if (prefix_len == 0) {
      // For empty prefix, include all non-empty history entries
      should_match = (line != prefix_str);
    } else {
      // For non-empty prefix, do case-insensitive prefix matching
      std::string line_lower;
      line_lower.reserve(line.length());
      std::transform(line.begin(), line.end(), std::back_inserter(line_lower),
                     [](unsigned char c) { return std::tolower(c); });
      should_match =
          (line_lower.rfind(prefix_lower, 0) == 0 && line != prefix_str);
    }

    if (should_match) {
      auto freq_it = g_completion_frequency.find(line);
      int frequency = (freq_it != g_completion_frequency.end()) ? freq_it->second : 1;
      matches.emplace_back(std::move(line), frequency);
    }
    
    // Clear line for reuse
    line.clear();
  }

  if (g_debug_mode)
    std::cerr << "DEBUG: Found " << matches.size()
              << " history matches for prefix: '" << prefix << "'" << std::endl;

  std::sort(matches.begin(), matches.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  const size_t max_suggestions = 15; // Reduce from 20 to 15
  size_t count = 0;

  for (const auto& match : matches) {
    // Check global completion limit
    if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
      if (g_debug_mode)
        std::cerr << "DEBUG: Reached completion limit in history suggestions" << std::endl;
      return;
    }
    
    // Always use the full command as completion text
    const std::string& completion = match.first;
    long delete_before = static_cast<long>(prefix_len);

    if (g_debug_mode)
      std::cerr << "DEBUG: Adding history completion: '" << match.first
                << "' -> '" << completion << "' (deleting " << delete_before 
                << " chars before, freq: " << match.second << ")" << std::endl;
    
    // Use safe wrapper function with source information
    if (!safe_add_completion_prim_with_source(cenv, completion.c_str(), nullptr, nullptr, "history", delete_before, 0))
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
    
  // Check global completion limit
  if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
    if (g_debug_mode)
      std::cerr << "DEBUG: Global completion limit reached, skipping filename completer" << std::endl;
    return;
  }

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
        size_t completion_count = 0;
        const size_t max_completions = 30; // Reduce from 100 to 30 to save memory
        
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          if (completion_count >= max_completions || ic_stop_completing(cenv)) {
            if (g_debug_mode && completion_count >= max_completions) {
              std::cerr << "DEBUG: Limiting tilde completions to " << max_completions << " entries" << std::endl;
            }
            break;
          }
          
          // Check global completion limit
          if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
            if (g_debug_mode)
              std::cerr << "DEBUG: Reached global completion limit in tilde completion" << std::endl;
            return;
          }
          
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

              const char* source = entry.is_directory() ? "directory" : "file";
              if (!safe_add_completion_with_source(cenv, completion_suffix.c_str(), source))
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

              if (!safe_add_completion_prim_with_source(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, entry.is_directory() ? "directory" : "file", delete_before, 0))
                return;
            }
            ++completion_count;
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
        size_t completion_count = 0;
        const size_t max_completions = 30; // Limit dash completions
        
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          if (completion_count >= max_completions || ic_stop_completing(cenv)) {
            if (g_debug_mode && completion_count >= max_completions) {
              std::cerr << "DEBUG: Limiting dash completions to " << max_completions << " entries" << std::endl;
            }
            break;
          }
          
          // Check global completion limit
          if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
            if (g_debug_mode)
              std::cerr << "DEBUG: Reached global completion limit in dash completion" << std::endl;
            return;
          }
          
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

              const char* source = entry.is_directory() ? "directory" : "file";
              if (!safe_add_completion_with_source(cenv, completion_suffix.c_str(), source))
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

              if (!safe_add_completion_prim_with_source(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, entry.is_directory() ? "directory" : "file", delete_before, 0))
                return;
            }
            ++completion_count;
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

              if (!safe_add_completion_prim_with_source(cenv, completion_text.c_str(), NULL, NULL, "bookmark", delete_before, 0))
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
        size_t completion_count = 0;
        const size_t max_completions = 30; // Limit all files completion
        
        for (auto& entry : fs::directory_iterator(dir_path)) {
          if (completion_count >= max_completions || ic_stop_completing(cenv)) {
            if (g_debug_mode && completion_count >= max_completions) {
              std::cerr << "DEBUG: Limiting all files completions to " << max_completions << " entries" << std::endl;
            }
            break;
          }
          
          // Check global completion limit
          if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
            if (g_debug_mode)
              std::cerr << "DEBUG: Reached global completion limit in all files completion" << std::endl;
            return;
          }
          
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
          if (!safe_add_completion_with_source(cenv, suffix.c_str(), entry.is_directory() ? "directory" : "file"))
            return;
          ++completion_count;
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
        size_t completion_count = 0;
        const size_t max_completions = 30; // Limit directory-only completion
        
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          if (completion_count >= max_completions || ic_stop_completing(cenv)) {
            if (g_debug_mode && completion_count >= max_completions) {
              std::cerr << "DEBUG: Limiting directory-only completions to " << max_completions << " entries" << std::endl;
            }
            break;
          }
          
          // Check global completion limit
          if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
            if (g_debug_mode)
              std::cerr << "DEBUG: Reached global completion limit in directory-only completion" << std::endl;
            return;
          }
          
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

              if (!safe_add_completion_with_source(cenv, completion_suffix.c_str(), "directory"))
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

              if (!safe_add_completion_prim_with_source(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, "directory", delete_before, 0))
                return;
            }
            ++completion_count;
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
        size_t completion_count = 0;
        const size_t max_completions = 30; // Limit general filename completion
        
        for (const auto& entry : fs::directory_iterator(dir_path)) {
          if (completion_count >= max_completions || ic_stop_completing(cenv)) {
            if (g_debug_mode && completion_count >= max_completions) {
              std::cerr << "DEBUG: Limiting general filename completions to " << max_completions << " entries" << std::endl;
            }
            break;
          }
          
          // Check global completion limit
          if (g_current_completion_tracker && g_current_completion_tracker->has_reached_completion_limit()) {
            if (g_debug_mode)
              std::cerr << "DEBUG: Reached global completion limit in general filename completion" << std::endl;
            return;
          }
          
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

              const char* source = entry.is_directory() ? "directory" : "file";
              if (!safe_add_completion_with_source(cenv, completion_suffix.c_str(), source))
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

              if (!safe_add_completion_prim_with_source(cenv, completion_suffix.c_str(),
                                          nullptr, nullptr, entry.is_directory() ? "directory" : "file", delete_before, 0))
                return;
            }
            ++completion_count;
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
  
  // Use RAII to ensure proper cleanup of completion session
  CompletionSession session(cenv, prefix);
  
  CompletionContext context = detect_completion_context(prefix);

  switch (context) {
    case CONTEXT_COMMAND:
      cjsh_history_completer(cenv, prefix);
      if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
        return;
      }

      cjsh_command_completer(cenv, prefix);
      if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
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
  
  // CompletionSession destructor will handle cleanup automatically
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

// Public function to force cleanup of completion system memory
void cleanup_completion_system() {
  if (g_debug_mode) {
    std::cerr << "DEBUG: Cleaning up completion system memory" << std::endl;
  }
  
  // Clean up any lingering completion tracker
  if (g_current_completion_tracker) {
    delete g_current_completion_tracker;
    g_current_completion_tracker = nullptr;
  }
  
  if (g_debug_mode) {
    std::cerr << "DEBUG: Completion system cleanup completed" << std::endl;
  }
}
