#include "cjsh_completions.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "cjsh_syntax_highlighter.h"
#include "cjshopt_command.h"
#include "plugin.h"
#include "shell.h"
#include "shell_script_interpreter.h"

std::map<std::string, int> g_completion_frequency;
bool g_completion_case_sensitive = false;
static const size_t MAX_COMPLETION_TRACKER_ENTRIES = 500;
static const size_t MAX_TOTAL_COMPLETIONS = 100;

enum CompletionContext {
    CONTEXT_COMMAND,
    CONTEXT_ARGUMENT,
    CONTEXT_PATH
};

enum SourcePriority {
    PRIORITY_HISTORY = 0,
    PRIORITY_BOOKMARK = 1,
    PRIORITY_UNKNOWN = 2,
    PRIORITY_FILE = 3,
    PRIORITY_DIRECTORY = 4,
    PRIORITY_PLUGIN = 5,
    PRIORITY_FUNCTION = 6
};

static SourcePriority get_source_priority(const char* source) {
    if (!source)
        return PRIORITY_UNKNOWN;

    if (strcmp(source, "history") == 0)
        return PRIORITY_HISTORY;
    if (strcmp(source, "bookmark") == 0)
        return PRIORITY_BOOKMARK;
    if (strcmp(source, "file") == 0)
        return PRIORITY_FILE;
    if (strcmp(source, "directory") == 0)
        return PRIORITY_DIRECTORY;
    if (strcmp(source, "plugin") == 0)
        return PRIORITY_PLUGIN;
    if (strcmp(source, "function") == 0)
        return PRIORITY_FUNCTION;

    return PRIORITY_UNKNOWN;
}

struct CompletionTracker {
    std::unordered_map<std::string, SourcePriority> added_completions;
    ic_completion_env_t* cenv;
    std::string original_prefix;
    size_t total_completions_added;

    CompletionTracker(ic_completion_env_t* env, const char* prefix)
        : cenv(env), original_prefix(prefix), total_completions_added(0) {
        added_completions.reserve(128);
    }

    ~CompletionTracker() {
        if (g_debug_mode && added_completions.size() > 1000) {
            std::cerr << "DEBUG: CompletionTracker had large size: "
                      << added_completions.size() << std::endl;
        }
        added_completions.clear();
    }

    bool has_reached_completion_limit() const {
        return total_completions_added >= MAX_TOTAL_COMPLETIONS;
    }

    std::string calculate_final_result(const char* completion_text,
                                       long delete_before = 0) {
        std::string prefix_str = original_prefix;

        if (delete_before > 0 &&
            delete_before <= static_cast<long>(prefix_str.length())) {
            prefix_str =
                prefix_str.substr(0, prefix_str.length() - delete_before);
        }

        return prefix_str + completion_text;
    }

    bool would_create_duplicate(const char* completion_text, const char* source,
                                long delete_before = 0) {
        if (added_completions.size() >= MAX_COMPLETION_TRACKER_ENTRIES) {
            return true;
        }

        std::string final_result =
            calculate_final_result(completion_text, delete_before);
        auto it = added_completions.find(final_result);

        if (it == added_completions.end()) {
            if (source && strcmp(source, "bookmark") == 0) {
                std::string directory_result = final_result + "/";
                auto dir_it = added_completions.find(directory_result);
                if (dir_it != added_completions.end()) {
                    return true;
                }
            } else if (source && strcmp(source, "directory") == 0) {
                std::string bookmark_result = final_result;
                if (bookmark_result.back() == '/') {
                    bookmark_result.pop_back();
                    auto bookmark_it = added_completions.find(bookmark_result);
                    if (bookmark_it != added_completions.end() &&
                        bookmark_it->second == PRIORITY_BOOKMARK) {
                        if (g_debug_mode) {
                            std::cerr << "DEBUG: Directory '" << completion_text
                                      << "' will replace existing bookmark "
                                         "with same name"
                                      << std::endl;
                        }

                        added_completions.erase(bookmark_it);
                    }
                }
            }

            return false;
        }

        SourcePriority existing_priority = it->second;
        SourcePriority new_priority = get_source_priority(source);

        return new_priority <= existing_priority;
    }

    bool add_completion_if_unique(const char* completion_text) {
        const char* source = nullptr;
        if (has_reached_completion_limit()) {
            return true;
        }
        if (would_create_duplicate(completion_text, source, 0)) {
            return true;
        }

        std::string final_result = calculate_final_result(completion_text, 0);
        added_completions[final_result] = get_source_priority(source);
        total_completions_added++;
        if (g_debug_mode)
            std::cerr << "DEBUG: Adding unique completion: '" << completion_text
                      << "' -> final: '" << final_result
                      << "' (total: " << total_completions_added << ")"
                      << std::endl;
        return ic_add_completion_ex_with_source(cenv, completion_text, nullptr,
                                                nullptr, source);
    }

    bool add_completion_prim_if_unique(const char* completion_text,
                                       const char* display, const char* help,
                                       long delete_before, long delete_after) {
        const char* source = nullptr;
        if (has_reached_completion_limit()) {
            return true;
        }
        if (would_create_duplicate(completion_text, source, delete_before)) {
            return true;
        }

        std::string final_result =
            calculate_final_result(completion_text, delete_before);
        added_completions[final_result] = get_source_priority(source);
        total_completions_added++;
        if (g_debug_mode)
            std::cerr << "DEBUG: Adding unique completion (prim): '"
                      << completion_text << "' -> final: '" << final_result
                      << "' (total: " << total_completions_added << ")"
                      << std::endl;
        return ic_add_completion_prim_with_source(cenv, completion_text,
                                                  display, help, source,
                                                  delete_before, delete_after);
    }

    bool add_completion_prim_with_source_if_unique(
        const char* completion_text, const char* display, const char* help,
        const char* source, long delete_before, long delete_after) {
        if (has_reached_completion_limit()) {
            return true;
        }

        std::string final_result =
            calculate_final_result(completion_text, delete_before);
        auto it = added_completions.find(final_result);
        SourcePriority new_priority = get_source_priority(source);

        if (it == added_completions.end()) {
            if (source && strcmp(source, "bookmark") == 0) {
                std::string directory_result = final_result + "/";
                auto dir_it = added_completions.find(directory_result);
                if (dir_it != added_completions.end()) {
                    return true;
                }
            } else if (source && strcmp(source, "directory") == 0) {
                std::string bookmark_result = final_result;
                if (bookmark_result.back() == '/') {
                    bookmark_result.pop_back();
                    auto bookmark_it = added_completions.find(bookmark_result);
                    if (bookmark_it != added_completions.end() &&
                        bookmark_it->second == PRIORITY_BOOKMARK) {
                        if (g_debug_mode) {
                            std::cerr << "DEBUG: Directory '" << completion_text
                                      << "' will replace existing bookmark "
                                         "with same name"
                                      << std::endl;
                        }

                        added_completions.erase(bookmark_it);
                    }
                }
            }

            total_completions_added++;
        } else {
            SourcePriority existing_priority = it->second;

            if (new_priority <= existing_priority) {
                return true;
            }

            if (g_debug_mode)
                std::cerr
                    << "DEBUG: Replacing lower priority completion with: '"
                    << completion_text << "' (source: '"
                    << (source ? source : "null")
                    << "', priority: " << new_priority
                    << " vs existing: " << existing_priority << ")"
                    << std::endl;
        }

        added_completions[final_result] = new_priority;
        if (g_debug_mode)
            std::cerr << "DEBUG: Adding unique completion (prim with source): '"
                      << completion_text << "' (source: '"
                      << (source ? source : "null") << "') -> final: '"
                      << final_result << "' (total: " << total_completions_added
                      << ")" << std::endl;
        return ic_add_completion_prim_with_source(cenv, completion_text,
                                                  display, help, source,
                                                  delete_before, delete_after);
    }
};

thread_local CompletionTracker* g_current_completion_tracker = nullptr;

class CompletionSession {
   public:
    CompletionSession(ic_completion_env_t* cenv, const char* prefix) {
        if (g_current_completion_tracker) {
            if (g_debug_mode) {
                std::cerr
                    << "DEBUG: Warning - completion tracker already exists, "
                       "cleaning up previous session"
                    << std::endl;
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

    CompletionSession(const CompletionSession&) = delete;
    CompletionSession& operator=(const CompletionSession&) = delete;
};

bool safe_add_completion(ic_completion_env_t* cenv,
                         const char* completion_text) {
    if (g_current_completion_tracker) {
        return g_current_completion_tracker->add_completion_if_unique(
            completion_text);
    } else {
        return ic_add_completion_ex_with_source(cenv, completion_text, nullptr,
                                                nullptr, nullptr);
    }
}

bool safe_add_completion_with_source(ic_completion_env_t* cenv,
                                     const char* completion_text,
                                     const char* source) {
    if (g_current_completion_tracker &&
        g_current_completion_tracker->has_reached_completion_limit()) {
        return true;
    }
    return ic_add_completion_ex_with_source(cenv, completion_text, nullptr,
                                            nullptr, source);
}

bool safe_add_completion_prim(ic_completion_env_t* cenv,
                              const char* completion_text, const char* display,
                              const char* help, long delete_before,
                              long delete_after) {
    if (g_current_completion_tracker) {
        return g_current_completion_tracker->add_completion_prim_if_unique(
            completion_text, display, help, delete_before, delete_after);
    } else {
        return ic_add_completion_prim_with_source(cenv, completion_text,
                                                  display, help, nullptr,
                                                  delete_before, delete_after);
    }
}

bool safe_add_completion_prim_with_source(ic_completion_env_t* cenv,
                                          const char* completion_text,
                                          const char* display, const char* help,
                                          const char* source,
                                          long delete_before,
                                          long delete_after) {
    if (g_current_completion_tracker) {
        return g_current_completion_tracker
            ->add_completion_prim_with_source_if_unique(
                completion_text, display, help, source, delete_before,
                delete_after);
    } else {
        return ic_add_completion_prim_with_source(cenv, completion_text,
                                                  display, help, source,
                                                  delete_before, delete_after);
    }
}

std::string quote_path_if_needed(const std::string& path);
bool starts_with_case_insensitive(const std::string& str,
                                  const std::string& prefix);
static bool matches_completion_prefix(const std::string& str,
                                      const std::string& prefix);
static bool equals_completion_token(const std::string& value,
                                    const std::string& target);
static bool starts_with_token(const std::string& value,
                              const std::string& target_prefix);

static bool completion_limit_hit() {
    return g_current_completion_tracker &&
           g_current_completion_tracker->has_reached_completion_limit();
}

static bool completion_limit_hit_with_log(const char* label) {
    if (!completion_limit_hit())
        return false;
    if (g_debug_mode)
        std::cerr << "DEBUG: Reached completion limit in " << label
                  << std::endl;
    return true;
}

static bool add_command_completion(ic_completion_env_t* cenv,
                                   const std::string& candidate,
                                   size_t prefix_len, const char* source,
                                   const char* debug_label) {
    long delete_before = static_cast<long>(prefix_len);
    if (g_debug_mode)
        std::cerr << "DEBUG: " << debug_label << " completion found: '"
                  << candidate << "' (deleting " << delete_before
                  << " chars before)" << std::endl;
    return safe_add_completion_prim_with_source(
        cenv, candidate.c_str(), nullptr, nullptr, source, delete_before, 0);
}

static std::string build_completion_suffix(
    const std::filesystem::directory_entry& entry) {
    std::string completion_suffix =
        quote_path_if_needed(entry.path().filename().string());
    if (entry.is_directory())
        completion_suffix += "/";
    return completion_suffix;
}

static bool add_path_completion(ic_completion_env_t* cenv,
                                const std::filesystem::directory_entry& entry,
                                long delete_before,
                                const std::string& completion_suffix) {
    const char* source = entry.is_directory() ? "directory" : "file";
    if (delete_before == 0)
        return safe_add_completion_with_source(cenv, completion_suffix.c_str(),
                                               source);
    return safe_add_completion_prim_with_source(cenv, completion_suffix.c_str(),
                                                nullptr, nullptr, source,
                                                delete_before, 0);
}

static void determine_directory_target(const std::string& path,
                                       bool treat_as_directory,
                                       std::filesystem::path& dir_path,
                                       std::string& match_prefix) {
    namespace fs = std::filesystem;
    if (treat_as_directory || path.empty() || path.back() == '/') {
        dir_path = path.empty() ? fs::path(".") : fs::path(path);
        match_prefix.clear();
        return;
    }
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string directory_part = path.substr(0, last_slash);
        if (directory_part.empty())
            directory_part = "/";
        dir_path = directory_part;
        match_prefix = path.substr(last_slash + 1);
    } else {
        dir_path = ".";
        match_prefix = path;
    }
}

template <typename Container, typename Extractor>
static void process_command_candidates(
    ic_completion_env_t* cenv, const Container& container,
    const std::string& prefix, size_t prefix_len, const char* source,
    const char* debug_label, Extractor extractor,
    const std::function<bool(const std::string&)>& filter = {}) {
    for (const auto& item : container) {
        if (completion_limit_hit_with_log(debug_label))
            return;
        if (ic_stop_completing(cenv))
            return;
        std::string candidate = extractor(item);
        if (filter && !filter(candidate))
            continue;
        if (!matches_completion_prefix(candidate, prefix))
            continue;
        if (!add_command_completion(cenv, candidate, prefix_len, source,
                                    debug_label))
            return;
        if (ic_stop_completing(cenv))
            return;
    }
}

static bool iterate_directory_entries(ic_completion_env_t* cenv,
                                      const std::filesystem::path& dir_path,
                                      const std::string& match_prefix,
                                      bool directories_only,
                                      size_t max_completions,
                                      bool skip_hidden_without_prefix,
                                      const char* debug_label) {
    namespace fs = std::filesystem;
    size_t completion_count = 0;
    std::string limit_label = std::string(debug_label) + " completion";
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (completion_count >= max_completions) {
            if (g_debug_mode)
                std::cerr << "DEBUG: Limiting " << debug_label
                          << " completions to " << max_completions << " entries"
                          << std::endl;
            break;
        }
        if (ic_stop_completing(cenv))
            return false;
        if (completion_limit_hit_with_log(limit_label.c_str()))
            return false;
        if (directories_only && !entry.is_directory())
            continue;
        std::string filename = entry.path().filename().string();
        if (filename.empty())
            continue;
        if (skip_hidden_without_prefix && match_prefix.empty() &&
            filename[0] == '.')
            continue;
        if (!match_prefix.empty() &&
            !matches_completion_prefix(filename, match_prefix))
            continue;
        long delete_before =
            match_prefix.empty() ? 0 : static_cast<long>(match_prefix.length());
        std::string completion_suffix = build_completion_suffix(entry);
        if (g_debug_mode) {
            std::cerr << "DEBUG: Adding " << debug_label;
            if (match_prefix.empty()) {
                std::cerr << " completion: '" << completion_suffix << "'";
            } else {
                std::cerr << " completion (full name): '" << completion_suffix
                          << "' (deleting " << delete_before
                          << " chars before)";
            }
            std::cerr << std::endl;
        }
        if (!add_path_completion(cenv, entry, delete_before, completion_suffix))
            return false;
        ++completion_count;
        if (ic_stop_completing(cenv))
            return false;
    }
    return true;
}

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
                current_token += c;
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

std::string quote_path_if_needed(const std::string& path) {
    if (path.empty())
        return path;

    bool needs_quoting = false;
    for (char c : path) {
        if (c == ' ' || c == '\t' || c == '\'' || c == '"' || c == '\\' ||
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' ||
            c == '}' || c == '&' || c == '|' || c == ';' || c == '<' ||
            c == '>' || c == '*' || c == '?' || c == '$' || c == '`') {
            needs_quoting = true;
            break;
        }
    }

    if (!needs_quoting)
        return path;

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

bool is_interactive_builtin(const std::string& cmd) {
    static const std::unordered_set<std::string> script_only_builtins = {
        "break",
        "continue",
        "return",
        "__INTERNAL_SUBSHELL__",
        "local",
        "shift",
        "if",
        "[[",
        "[",
        ":",
        "login-startup-arg",
        "prompt_test"};

    return script_only_builtins.find(cmd) == script_only_builtins.end();
}

CompletionContext detect_completion_context(const char* prefix) {
    std::string prefix_str(prefix);

    if (g_debug_mode)
        std::cerr << "DEBUG: Detecting completion context for prefix: '"
                  << prefix << "'" << std::endl;

    if (prefix_str.find('/') == 0 || prefix_str.find("./") == 0 ||
        prefix_str.find("../") == 0) {
        if (g_debug_mode)
            std::cerr << "DEBUG: Context detected: PATH" << std::endl;
        return CONTEXT_PATH;
    }

    std::vector<std::string> tokens = tokenize_command_line(prefix_str);

    if (tokens.size() > 1) {
        if (g_debug_mode)
            std::cerr << "DEBUG: Context detected: ARGUMENT (found "
                      << tokens.size() << " tokens)" << std::endl;
        return CONTEXT_ARGUMENT;
    }

    size_t last_unquoted_space = find_last_unquoted_space(prefix_str);
    if (last_unquoted_space != std::string::npos) {
        if (g_debug_mode)
            std::cerr << "DEBUG: Context detected: ARGUMENT (incomplete token "
                         "with spaces)"
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

    if (completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    size_t prefix_len = prefix_str.length();

    std::vector<std::string> builtin_cmds;
    std::vector<std::string> function_names;
    std::vector<std::string> plugin_cmds;
    std::unordered_set<std::string> aliases;
    std::vector<std::filesystem::path> cached_executables;

    if (g_shell && g_shell->get_built_ins()) {
        builtin_cmds = g_shell->get_built_ins()->get_builtin_commands();
    }

    if (g_shell && g_shell->get_shell_script_interpreter()) {
        function_names =
            g_shell->get_shell_script_interpreter()->get_function_names();
    }

    if (g_plugin) {
        auto enabled_plugins = g_plugin->get_enabled_plugins();
        for (const auto& plugin : enabled_plugins) {
            auto plugin_commands = g_plugin->get_plugin_commands(plugin);
            plugin_cmds.insert(plugin_cmds.end(), plugin_commands.begin(),
                               plugin_commands.end());
        }
    }

    if (g_shell) {
        auto shell_aliases = g_shell->get_aliases();
        for (const auto& alias : shell_aliases) {
            aliases.insert(alias.first);
        }
    }

    cached_executables = cjsh_filesystem::read_cached_executables();

    auto builtin_filter = [&](const std::string& cmd) {
        if (is_interactive_builtin(cmd))
            return true;

        return false;
    };

    process_command_candidates(
        cenv, builtin_cmds, prefix_str, prefix_len, "builtin",
        "builtin commands", [](const std::string& value) { return value; },
        builtin_filter);
    if (completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, function_names, prefix_str, prefix_len,
                               "function", "function commands",
                               [](const std::string& value) { return value; });
    if (completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, plugin_cmds, prefix_str, prefix_len,
                               "plugin", "plugin commands",
                               [](const std::string& value) { return value; });
    if (completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, aliases, prefix_str, prefix_len, "alias",
                               "aliases",
                               [](const std::string& value) { return value; });
    if (completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, cached_executables, prefix_str, prefix_len,
                               "system", "cached executables",
                               [](const std::filesystem::path& value) {
                                   return value.filename().string();
                               });

    if (g_debug_mode && !ic_has_completions(cenv))
        std::cerr << "DEBUG: No command completions found for prefix: '"
                  << prefix << "'" << std::endl;
}

bool looks_like_file_path(const std::string& str) {
    if (str.empty())
        return false;

    if (str[0] == '/' || str.rfind("./", 0) == 0 || str.rfind("../", 0) == 0 ||
        str.rfind("~/", 0) == 0 || str.find('/') != std::string::npos) {
        return true;
    }

    size_t dot_pos = str.rfind('.');
    if (dot_pos != std::string::npos && dot_pos > 0 &&
        dot_pos < str.length() - 1) {
        std::string extension = str.substr(dot_pos + 1);

        static const std::unordered_set<std::string> file_extensions = {
            "txt",  "log",  "conf", "config", "json", "xml",  "yaml", "yml",
            "cpp",  "c",    "h",    "hpp",    "py",   "js",   "ts",   "java",
            "sh",   "bash", "md",   "html",   "css",  "sql",  "tar",  "gz",
            "zip",  "pdf",  "doc",  "docx",   "xls",  "xlsx", "png",  "jpg",
            "jpeg", "gif",  "mp3",  "mp4"};

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

    if (completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    size_t prefix_len = prefix_str.length();

    if (prefix_len == 0) {
    }

    std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path);
    if (!history_file.is_open()) {
        if (g_debug_mode)
            std::cerr << "DEBUG: Failed to open history file: "
                      << cjsh_filesystem::g_cjsh_history_path << std::endl;
        return;
    }

    std::vector<std::pair<std::string, int>> matches;
    matches.reserve(50);

    std::string line;
    line.reserve(256);

    while (std::getline(history_file, line) && matches.size() < 50) {
        if (line.empty())
            continue;

        if (line.length() > 1 && line[0] == '#' && line[1] == ' ') {
            continue;
        }

        if (looks_like_file_path(line)) {
            continue;
        }

        bool should_match = false;
        if (prefix_len == 0) {
            should_match = (line != prefix_str);
        } else if (matches_completion_prefix(line, prefix_str) &&
                   line != prefix_str) {
            should_match = true;
        }

        if (should_match) {
            auto freq_it = g_completion_frequency.find(line);
            int frequency =
                (freq_it != g_completion_frequency.end()) ? freq_it->second : 1;
            matches.emplace_back(std::move(line), frequency);
        }

        line.clear();
    }

    if (g_debug_mode)
        std::cerr << "DEBUG: Found " << matches.size()
                  << " history matches for prefix: '" << prefix << "'"
                  << std::endl;

    std::sort(matches.begin(), matches.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    const size_t max_suggestions = 15;
    size_t count = 0;

    for (const auto& match : matches) {
        if (completion_limit_hit_with_log("history suggestions"))
            return;

        const std::string& completion = match.first;
        long delete_before = static_cast<long>(prefix_len);

        if (g_debug_mode)
            std::cerr << "DEBUG: Adding history completion: '" << match.first
                      << "' -> '" << completion << "' (deleting "
                      << delete_before
                      << " chars before, freq: " << match.second << ")"
                      << std::endl;

        if (!safe_add_completion_prim_with_source(cenv, completion.c_str(),
                                                  nullptr, nullptr, "history",
                                                  delete_before, 0))
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
    if (g_completion_case_sensitive) {
        return directory_only_commands.find(command) !=
               directory_only_commands.end();
    }

    std::string lowered_command = command;
    std::transform(lowered_command.begin(), lowered_command.end(),
                   lowered_command.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return directory_only_commands.find(lowered_command) !=
           directory_only_commands.end();
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

static bool starts_with_case_sensitive(const std::string& str,
                                       const std::string& prefix) {
    if (prefix.length() > str.length()) {
        return false;
    }

    return std::equal(prefix.begin(), prefix.end(), str.begin());
}

static bool matches_completion_prefix(const std::string& str,
                                      const std::string& prefix) {
    if (g_completion_case_sensitive) {
        return starts_with_case_sensitive(str, prefix);
    }

    return starts_with_case_insensitive(str, prefix);
}

static bool equals_completion_token(const std::string& value,
                                    const std::string& target) {
    if (g_completion_case_sensitive) {
        return value == target;
    }

    if (value.length() != target.length()) {
        return false;
    }

    return std::equal(
        value.begin(), value.end(), target.begin(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

static bool starts_with_token(const std::string& value,
                              const std::string& target_prefix) {
    if (g_completion_case_sensitive) {
        return starts_with_case_sensitive(value, target_prefix);
    }

    return starts_with_case_insensitive(value, target_prefix);
}

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
    if (g_debug_mode)
        std::cerr << "DEBUG: Filename completer called with prefix: '" << prefix
                  << "'" << std::endl;

    if (ic_stop_completing(cenv))
        return;

    if (completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    bool directories_only = should_complete_directories_only(prefix_str);

    if (g_debug_mode && directories_only)
        std::cerr
            << "DEBUG: Directory-only completion mode enabled for prefix: '"
            << prefix << "'" << std::endl;

    size_t last_space = find_last_unquoted_space(prefix_str);

    bool has_tilde = false;
    bool has_dash = false;
    std::string prefix_before;
    std::string special_part;

    if (last_space != std::string::npos &&
        last_space + 1 < prefix_str.length()) {
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
    } else if (!prefix_str.empty() && prefix_str[0] == '~') {
        has_tilde = true;
        special_part = prefix_str;
    } else if (!prefix_str.empty() && prefix_str[0] == '-' &&
               (prefix_str.length() == 1 || prefix_str[1] == '/')) {
        has_dash = true;
        special_part = prefix_str;
    } else if (starts_with_token(prefix_str, "cd ") &&
               prefix_str.length() > 3) {
        prefix_before = prefix_str.substr(0, 3);
        special_part = prefix_str.substr(3);
    }

    if (has_tilde && (special_part.length() == 1 || special_part[1] == '/')) {
        if (g_debug_mode)
            std::cerr << "DEBUG: Processing tilde completion: '" << special_part
                      << "'" << std::endl;

        std::string unquoted_special = unquote_path(special_part);
        std::string path_after_tilde =
            unquoted_special.length() > 1 ? unquoted_special.substr(2) : "";
        std::string dir_to_complete =
            cjsh_filesystem::g_user_home_path.string();

        if (unquoted_special.length() > 1) {
            dir_to_complete += "/" + path_after_tilde;
        }

        namespace fs = std::filesystem;
        fs::path dir_path;
        std::string match_prefix;
        bool treat_as_directory =
            !unquoted_special.empty() && unquoted_special.back() == '/';
        determine_directory_target(dir_to_complete, treat_as_directory,
                                   dir_path, match_prefix);

        if (g_debug_mode) {
            std::cerr << "DEBUG: Looking in directory: '" << dir_path << "'"
                      << std::endl;
            std::cerr << "DEBUG: Matching prefix: '" << match_prefix << "'"
                      << std::endl;
        }

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix,
                                               false, 30, false, "tilde"))
                    return;
            }
        } catch (const std::exception& e) {
            if (g_debug_mode)
                std::cerr << "DEBUG: Error reading directory: " << e.what()
                          << std::endl;
        }

        return;
    } else if (has_dash &&
               (special_part.length() == 1 || special_part[1] == '/')) {
        if (g_debug_mode)
            std::cerr
                << "DEBUG: Processing dash completion for previous directory: '"
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
        bool treat_as_directory =
            !unquoted_special.empty() && unquoted_special.back() == '/';
        determine_directory_target(dir_to_complete, treat_as_directory,
                                   dir_path, match_prefix);

        if (g_debug_mode) {
            std::cerr << "DEBUG: Looking in directory: '" << dir_path << "'"
                      << std::endl;
            std::cerr << "DEBUG: Matching prefix: '" << match_prefix << "'"
                      << std::endl;
        }

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix,
                                               false, 30, false, "dash"))
                    return;
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

        if (equals_completion_token(command_part, "cd") ||
            starts_with_token(command_part, "cd ")) {
            if (!config::smart_cd_enabled) {
            } else {
                if (g_debug_mode)
                    std::cerr << "DEBUG: Processing bookmark completions for "
                                 "cd command "
                                 "with prefix: '"
                              << special_part << "'" << std::endl;

                if (g_shell && g_shell->get_built_ins()) {
                    const auto& bookmarks =
                        g_shell->get_built_ins()->get_directory_bookmarks();
                    std::string bookmark_match_prefix =
                        unquote_path(special_part);

                    for (const auto& bookmark : bookmarks) {
                        const std::string& bookmark_name = bookmark.first;
                        const std::string& bookmark_path = bookmark.second;

                        if (bookmark_match_prefix.empty() ||
                            matches_completion_prefix(bookmark_name,
                                                      bookmark_match_prefix)) {
                            namespace fs = std::filesystem;
                            if (fs::exists(bookmark_path) &&
                                fs::is_directory(bookmark_path)) {
                                std::string current_dir_item =
                                    "./" + bookmark_name;
                                if (fs::exists(current_dir_item) &&
                                    fs::is_directory(current_dir_item)) {
                                    continue;
                                }

                                size_t delete_before = special_part.length();

                                std::string completion_text = bookmark_name;

                                if (g_debug_mode)
                                    std::cerr << "DEBUG: Adding bookmark "
                                                 "completion: '"
                                              << bookmark_name << "' -> '"
                                              << completion_text
                                              << "' (deleting " << delete_before
                                              << " chars before)" << std::endl;

                                if (!safe_add_completion_prim_with_source(
                                        cenv, completion_text.c_str(), NULL,
                                        NULL, "bookmark", delete_before, 0))
                                    return;
                            }
                        }
                    }
                }
            }
        }
    }

    std::string path_to_check = special_part.empty()
                                    ? unquote_path(prefix_str)
                                    : unquote_path(special_part);

    if (!ic_stop_completing(cenv) && !path_to_check.empty() &&
        path_to_check.back() == '/') {
        namespace fs = std::filesystem;
        fs::path dir_path(path_to_check);
        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, "",
                                               directories_only, 30, false,
                                               "all files"))
                    return;
            }
        } catch (const std::exception& e) {
            if (g_debug_mode)
                std::cerr << "DEBUG: Error reading directory for all files "
                             "completion: "
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
        bool treat_as_directory =
            path_to_complete.empty() || path_to_complete.back() == '/';
        determine_directory_target(path_to_complete, treat_as_directory,
                                   dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix,
                                               true, 30, true,
                                               "directory-only"))
                    return;
            }
        } catch (const std::exception& e) {
            if (g_debug_mode)
                std::cerr << "DEBUG: Error in directory-only completion: "
                          << e.what() << std::endl;
        }
    } else {
        std::string path_to_complete = special_part.empty()
                                           ? unquote_path(prefix_str)
                                           : unquote_path(special_part);

        if (g_debug_mode)
            std::cerr
                << "DEBUG: General filename completion for unquoted path: '"
                << path_to_complete << "'" << std::endl;

        namespace fs = std::filesystem;
        fs::path dir_path;
        std::string match_prefix;
        bool treat_as_directory =
            path_to_complete.empty() || path_to_complete.back() == '/';
        determine_directory_target(path_to_complete, treat_as_directory,
                                   dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix,
                                               false, 30, true,
                                               "general filename"))
                    return;
            }
        } catch (const std::exception& e) {
            if (g_debug_mode)
                std::cerr << "DEBUG: Error in general filename completion: "
                          << e.what() << std::endl;
        }
    }

    if (g_debug_mode) {
        if (ic_has_completions(cenv))
            std::cerr << "DEBUG: Filename completions found for prefix: '"
                      << prefix << "'" << std::endl;
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
            std::string prefix_str(prefix);
            std::vector<std::string> tokens = tokenize_command_line(prefix_str);

            if (!tokens.empty() && equals_completion_token(tokens[0], "cd")) {
                if (g_debug_mode)
                    std::cerr << "DEBUG: Detected cd command, using only "
                                 "filename completion"
                              << std::endl;
                cjsh_filename_completer(cenv, prefix);
            } else {
                cjsh_history_completer(cenv, prefix);
                cjsh_filename_completer(cenv, prefix);
            }
            break;
        }
    }
}

void initialize_completion_system() {
    if (g_debug_mode)
        std::cerr << "DEBUG: Initializing completion system" << std::endl;

    reset_to_default_styles();

    load_custom_styles_from_config();

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

void cleanup_completion_system() {
    if (g_debug_mode) {
        std::cerr << "DEBUG: Cleaning up completion system memory" << std::endl;
    }

    if (g_current_completion_tracker) {
        delete g_current_completion_tracker;
        g_current_completion_tracker = nullptr;
    }

    if (g_debug_mode) {
        std::cerr << "DEBUG: Completion system cleanup completed" << std::endl;
    }
}

void set_completion_case_sensitive(bool case_sensitive) {
    g_completion_case_sensitive = case_sensitive;
}

bool is_completion_case_sensitive() {
    return g_completion_case_sensitive;
}

void refresh_cached_executables() {
    if (g_debug_mode) {
        std::cerr << "DEBUG: Cached executables refreshed for completion system"
                  << std::endl;
    }
}
