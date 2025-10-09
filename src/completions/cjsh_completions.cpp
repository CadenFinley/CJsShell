#include "cjsh_completions.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "isocline.h"
#include "shell.h"
#include "shell_script_interpreter.h"

std::map<std::string, int> g_completion_frequency;
bool g_completion_case_sensitive = false;
bool g_completion_spell_correction_enabled = true;  // NOLINT
static const size_t MAX_COMPLETION_TRACKER_ENTRIES = 250;
static const size_t MAX_TOTAL_COMPLETIONS = 50;

enum CompletionContext : std::uint8_t {
    CONTEXT_COMMAND,
    CONTEXT_ARGUMENT,
    CONTEXT_PATH
};

enum SourcePriority : std::uint8_t {
    PRIORITY_HISTORY = 0,
    PRIORITY_BOOKMARK = 1,
    PRIORITY_UNKNOWN = 2,
    PRIORITY_FILE = 3,
    PRIORITY_DIRECTORY = 4,
    PRIORITY_FUNCTION = 5
};

static SourcePriority get_source_priority(const char* source) {
    if (source == nullptr)
        return PRIORITY_UNKNOWN;

    if (strcmp(source, "history") == 0)
        return PRIORITY_HISTORY;
    if (strcmp(source, "bookmark") == 0)
        return PRIORITY_BOOKMARK;
    if (strcmp(source, "file") == 0)
        return PRIORITY_FILE;
    if (strcmp(source, "directory") == 0)
        return PRIORITY_DIRECTORY;
    if (strcmp(source, "function") == 0)
        return PRIORITY_FUNCTION;

    return PRIORITY_UNKNOWN;
}

static const char* extract_current_line_prefix(const char* prefix) {
    if (prefix == nullptr) {
        return "";
    }

    const char* last_newline = strrchr(prefix, '\n');
    const char* last_carriage = strrchr(prefix, '\r');
    const char* last_break = last_newline;

    if (last_carriage != nullptr && (last_break == nullptr || last_carriage > last_break)) {
        last_break = last_carriage;
    }

    if (last_break != nullptr) {
        return last_break + 1;
    }

    return prefix;
}

namespace {
constexpr long kHistoryMinEntries = 0;
constexpr long kHistoryDefaultEntries = 200;
constexpr long kHistoryAbsoluteMaxEntries = 5000;

long g_history_max_entries_value = kHistoryDefaultEntries;

struct SerializedHistoryEntry {
    std::string timestamp;
    std::string payload;
};

bool trim_history_file(long max_entries, std::string* error_message) {
    if (max_entries < 0) {
        return true;
    }

    const auto& history_path = cjsh_filesystem::g_cjsh_history_path;

    if (max_entries == 0) {
        std::error_code remove_ec;
        std::filesystem::remove(history_path, remove_ec);
        if (remove_ec && remove_ec != std::errc::no_such_file_or_directory) {
            if (error_message != nullptr) {
                *error_message = "Failed to remove history file '" + history_path.string() +
                                 "': " + remove_ec.message();
            }
            return false;
        }
        return true;
    }

    std::error_code exists_ec;
    if (!std::filesystem::exists(history_path, exists_ec)) {
        if (exists_ec) {
            if (error_message != nullptr) {
                *error_message = "Failed to inspect history file '" + history_path.string() +
                                 "': " + exists_ec.message();
            }
            return false;
        }
        return true;
    }

    std::ifstream history_stream(history_path);
    if (!history_stream.is_open()) {
        if (error_message != nullptr) {
            *error_message =
                "Failed to open history file '" + history_path.string() + "' for reading.";
        }
        return false;
    }

    std::vector<SerializedHistoryEntry> entries;
    entries.reserve(static_cast<size_t>(max_entries) + 16);

    std::string line;
    SerializedHistoryEntry current;
    bool seen_timestamp = false;

    while (std::getline(history_stream, line)) {
        if (!line.empty() && line[0] == '#') {
            if (seen_timestamp && !current.timestamp.empty()) {
                entries.push_back(current);
                current = SerializedHistoryEntry{};
            }
            current.timestamp = line;
            current.payload.clear();
            seen_timestamp = true;
        } else {
            if (!seen_timestamp) {
                continue;
            }
            if (!current.payload.empty()) {
                current.payload += '\n';
            }
            current.payload += line;
        }
    }

    if (seen_timestamp && !current.timestamp.empty()) {
        entries.push_back(current);
    }

    history_stream.close();

    if (entries.size() <= static_cast<size_t>(max_entries)) {
        return true;
    }

    size_t start_index = entries.size() - static_cast<size_t>(max_entries);
    std::ostringstream buffer;

    for (size_t i = start_index; i < entries.size(); ++i) {
        buffer << entries[i].timestamp << '\n';
        if (!entries[i].payload.empty()) {
            buffer << entries[i].payload;
        }
        buffer << '\n';
    }

    auto write_result = cjsh_filesystem::write_file_content(history_path.string(), buffer.str());
    if (write_result.is_error()) {
        if (error_message != nullptr) {
            *error_message = write_result.error();
        }
        return false;
    }

    return true;
}

bool enforce_history_limit_internal(std::string* error_message) {
    if (g_history_max_entries_value <= 0) {
        ic_set_history(nullptr, 0);
        return trim_history_file(0, error_message);
    }

    ic_set_history(cjsh_filesystem::g_cjsh_history_path.c_str(), g_history_max_entries_value);
    return trim_history_file(g_history_max_entries_value, error_message);
}
}  // namespace

struct CompletionTracker {
    std::unordered_map<std::string, SourcePriority> added_completions;
    ic_completion_env_t* cenv;
    std::string original_prefix;
    size_t total_completions_added{};

    CompletionTracker(ic_completion_env_t* env, const char* prefix)
        : cenv(env), original_prefix(prefix) {
        added_completions.reserve(128);
    }

    ~CompletionTracker() {
        added_completions.clear();
    }

    bool has_reached_completion_limit() const {
        return total_completions_added >= MAX_TOTAL_COMPLETIONS;
    }

    std::string calculate_final_result(const char* completion_text, long delete_before = 0) const {
        std::string prefix_str = original_prefix;

        if (delete_before > 0 && delete_before <= static_cast<long>(prefix_str.length())) {
            prefix_str = prefix_str.substr(0, prefix_str.length() - delete_before);
        }

        return prefix_str + completion_text;
    }

    bool would_create_duplicate(const char* completion_text, const char* source,
                                long delete_before = 0) {
        if (added_completions.size() >= MAX_COMPLETION_TRACKER_ENTRIES) {
            return true;
        }

        std::string final_result = calculate_final_result(completion_text, delete_before);
        auto it = added_completions.find(final_result);

        if (it == added_completions.end()) {
            if ((source != nullptr) && strcmp(source, "bookmark") == 0) {
                std::string directory_result = final_result + "/";
                auto dir_it = added_completions.find(directory_result);
                if (dir_it != added_completions.end()) {
                    return true;
                }
            } else if ((source != nullptr) && strcmp(source, "directory") == 0) {
                std::string bookmark_result = final_result;
                if (bookmark_result.back() == '/') {
                    bookmark_result.pop_back();
                    auto bookmark_it = added_completions.find(bookmark_result);
                    if (bookmark_it != added_completions.end() &&
                        bookmark_it->second == PRIORITY_BOOKMARK) {
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
        return ic_add_completion_ex_with_source(cenv, completion_text, nullptr, nullptr, source);
    }

    bool add_completion_prim_if_unique(const char* completion_text, const char* display,
                                       const char* help, long delete_before, long delete_after) {
        const char* source = nullptr;
        if (has_reached_completion_limit()) {
            return true;
        }
        if (would_create_duplicate(completion_text, source, delete_before)) {
            return true;
        }

        std::string final_result = calculate_final_result(completion_text, delete_before);
        added_completions[final_result] = get_source_priority(source);
        total_completions_added++;
        return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source,
                                                  delete_before, delete_after);
    }

    bool add_completion_prim_with_source_if_unique(const char* completion_text, const char* display,
                                                   const char* help, const char* source,
                                                   long delete_before, long delete_after) {
        if (has_reached_completion_limit()) {
            return true;
        }

        std::string final_result = calculate_final_result(completion_text, delete_before);
        auto it = added_completions.find(final_result);
        SourcePriority new_priority = get_source_priority(source);

        if (it == added_completions.end()) {
            if ((source != nullptr) && strcmp(source, "bookmark") == 0) {
                std::string directory_result = final_result + "/";
                auto dir_it = added_completions.find(directory_result);
                if (dir_it != added_completions.end()) {
                    return true;
                }
            } else if ((source != nullptr) && strcmp(source, "directory") == 0) {
                std::string bookmark_result = final_result;
                if (bookmark_result.back() == '/') {
                    bookmark_result.pop_back();
                    auto bookmark_it = added_completions.find(bookmark_result);
                    if (bookmark_it != added_completions.end() &&
                        bookmark_it->second == PRIORITY_BOOKMARK) {
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
        }

        added_completions[final_result] = new_priority;
        return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source,
                                                  delete_before, delete_after);
    }
};

thread_local static CompletionTracker* g_current_completion_tracker = nullptr;

static void completion_session_begin(ic_completion_env_t* cenv, const char* prefix) {
    delete g_current_completion_tracker;
    g_current_completion_tracker = new CompletionTracker(cenv, prefix);
}

static void completion_session_end() {
    if (g_current_completion_tracker != nullptr) {
        delete g_current_completion_tracker;
        g_current_completion_tracker = nullptr;
    }
}

static bool safe_add_completion_with_source(ic_completion_env_t* cenv, const char* completion_text,
                                            const char* source) {
    if ((g_current_completion_tracker != nullptr) &&
        g_current_completion_tracker->has_reached_completion_limit()) {
        return true;
    }
    return ic_add_completion_ex_with_source(cenv, completion_text, nullptr, nullptr, source);
}

static bool safe_add_completion_prim_with_source(ic_completion_env_t* cenv,
                                                 const char* completion_text, const char* display,
                                                 const char* help, const char* source,
                                                 long delete_before, long delete_after) {
    if (g_current_completion_tracker != nullptr) {
        return g_current_completion_tracker->add_completion_prim_with_source_if_unique(
            completion_text, display, help, source, delete_before, delete_after);
    }
    return ic_add_completion_prim_with_source(cenv, completion_text, display, help, source,
                                              delete_before, delete_after);
}

static std::string quote_path_if_needed(const std::string& path);
static bool starts_with_case_insensitive(const std::string& str, const std::string& prefix);
static bool matches_completion_prefix(const std::string& str, const std::string& prefix);
static bool equals_completion_token(const std::string& value, const std::string& target);
static bool starts_with_token(const std::string& value, const std::string& target_prefix);

static bool completion_limit_hit() {
    return (g_current_completion_tracker != nullptr) &&
           g_current_completion_tracker->has_reached_completion_limit();
}

static bool completion_limit_hit_with_log(const char* label) {
    (void)label;
    return completion_limit_hit();
}

static bool add_command_completion(ic_completion_env_t* cenv, const std::string& candidate,
                                   size_t prefix_len, const char* source, const char* debug_label) {
    (void)debug_label;
    long delete_before = static_cast<long>(prefix_len);
    return safe_add_completion_prim_with_source(cenv, candidate.c_str(), nullptr, nullptr, source,
                                                delete_before, 0);
}

static std::string build_completion_suffix(const std::filesystem::directory_entry& entry) {
    std::string completion_suffix = quote_path_if_needed(entry.path().filename().string());
    if (entry.is_directory())
        completion_suffix += "/";
    return completion_suffix;
}

static bool add_path_completion(ic_completion_env_t* cenv,
                                const std::filesystem::directory_entry& entry, long delete_before,
                                const std::string& completion_suffix) {
    const char* source = entry.is_directory() ? "directory" : "file";
    if (delete_before == 0)
        return safe_add_completion_with_source(cenv, completion_suffix.c_str(), source);
    return safe_add_completion_prim_with_source(cenv, completion_suffix.c_str(), nullptr, nullptr,
                                                source, delete_before, 0);
}

static void determine_directory_target(const std::string& path, bool treat_as_directory,
                                       std::filesystem::path& dir_path, std::string& match_prefix) {
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
static void process_command_candidates(ic_completion_env_t* cenv, const Container& container,
                                       const std::string& prefix, size_t prefix_len,
                                       const char* source, const char* debug_label,
                                       Extractor extractor,
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
        if (!add_command_completion(cenv, candidate, prefix_len, source, debug_label))
            return;
        if (ic_stop_completing(cenv))
            return;
    }
}

static bool iterate_directory_entries(ic_completion_env_t* cenv,
                                      const std::filesystem::path& dir_path,
                                      const std::string& match_prefix, bool directories_only,
                                      size_t max_completions, bool skip_hidden_without_prefix,
                                      const char* debug_label) {
    namespace fs = std::filesystem;
    size_t completion_count = 0;
    std::string limit_label = std::string(debug_label) + " completion";
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (completion_count >= max_completions) {
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
        if (skip_hidden_without_prefix && match_prefix.empty() && filename[0] == '.')
            continue;
        if (!match_prefix.empty() && !matches_completion_prefix(filename, match_prefix))
            continue;
        long delete_before = match_prefix.empty() ? 0 : static_cast<long>(match_prefix.length());
        std::string completion_suffix = build_completion_suffix(entry);
        if (!add_path_completion(cenv, entry, delete_before, completion_suffix))
            return false;
        ++completion_count;
        if (ic_stop_completing(cenv))
            return false;
    }
    return true;
}

static size_t find_last_unquoted_space(const std::string& str) {
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

static std::string normalize_for_comparison(const std::string& value) {
    if (g_completion_case_sensitive) {
        return value;
    }

    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower_value;
}

static bool is_adjacent_transposition(const std::string& a, const std::string& b) {
    if (a.length() != b.length() || a.length() < 2) {
        return false;
    }

    size_t first_diff = std::string::npos;

    for (size_t i = 0; i < a.length(); ++i) {
        if (a[i] != b[i]) {
            if (first_diff == std::string::npos) {
                first_diff = i;
            } else {
                if (i == first_diff + 1 && a[first_diff] == b[i] && a[i] == b[first_diff]) {
                    for (size_t k = i + 1; k < a.length(); ++k) {
                        if (a[k] != b[k]) {
                            return false;
                        }
                    }
                    return true;
                }
                return false;
            }
        }
    }

    return false;
}

static int compute_edit_distance_with_limit(const std::string& source, const std::string& target,
                                            int max_distance) {
    const size_t source_length = source.length();
    const size_t target_length = target.length();

    if (std::abs(static_cast<int>(source_length) - static_cast<int>(target_length)) >
        max_distance) {
        return max_distance + 1;
    }

    std::vector<int> previous_row(target_length + 1);
    std::vector<int> current_row(target_length + 1);

    for (size_t j = 0; j <= target_length; ++j) {
        previous_row[j] = static_cast<int>(j);
    }

    for (size_t i = 1; i <= source_length; ++i) {
        current_row[0] = static_cast<int>(i);
        int row_min = current_row[0];

        for (size_t j = 1; j <= target_length; ++j) {
            int cost = (source[i - 1] == target[j - 1]) ? 0 : 1;
            current_row[j] =
                std::min({previous_row[j] + 1, current_row[j - 1] + 1, previous_row[j - 1] + cost});
            row_min = std::min(row_min, current_row[j]);
        }

        if (row_min > max_distance) {
            return max_distance + 1;
        }

        std::swap(previous_row, current_row);
    }

    return previous_row[target_length];
}

static bool should_consider_spell_correction(const std::string& normalized_prefix) {
    return normalized_prefix.length() >= 2;
}

struct SpellCorrectionMatch {
    std::string candidate;
    int distance{};
    bool is_transposition{};
};

template <typename Container, typename Extractor>
static void collect_spell_correction_candidates(
    const Container& container, Extractor extractor,
    const std::function<bool(const std::string&)>& filter, const std::string& normalized_prefix,
    std::unordered_map<std::string, SpellCorrectionMatch>& matches) {
    for (const auto& item : container) {
        std::string candidate = extractor(item);
        if (filter && !filter(candidate)) {
            continue;
        }

        std::string normalized_candidate = normalize_for_comparison(candidate);
        if (normalized_candidate == normalized_prefix) {
            continue;
        }

        bool is_transposition_match =
            is_adjacent_transposition(normalized_candidate, normalized_prefix);
        int distance = compute_edit_distance_with_limit(normalized_candidate, normalized_prefix, 2);
        if (!is_transposition_match && distance > 2) {
            continue;
        }

        int effective_distance = is_transposition_match ? 1 : distance;
        auto it = matches.find(candidate);

        if (it == matches.end() || effective_distance < it->second.distance) {
            matches[candidate] =
                SpellCorrectionMatch{candidate, effective_distance, is_transposition_match};
        }
    }
}

static void add_spell_correction_matches(
    ic_completion_env_t* cenv, const std::unordered_map<std::string, SpellCorrectionMatch>& matches,
    size_t prefix_length) {
    std::vector<SpellCorrectionMatch> ordered_matches;
    ordered_matches.reserve(matches.size());

    for (const auto& entry : matches) {
        ordered_matches.push_back(entry.second);
    }

    std::sort(ordered_matches.begin(), ordered_matches.end(),
              [](const SpellCorrectionMatch& a, const SpellCorrectionMatch& b) {
                  if (a.distance != b.distance) {
                      return a.distance < b.distance;
                  }
                  if (a.is_transposition != b.is_transposition) {
                      return a.is_transposition && !b.is_transposition;
                  }
                  return a.candidate < b.candidate;
              });

    const size_t kMaxSpellMatches = 10;
    size_t added = 0;

    for (const auto& match : ordered_matches) {
        if (completion_limit_hit_with_log("spell correction")) {
            return;
        }
        if (!add_command_completion(cenv, match.candidate, prefix_length, "spell",
                                    "spell correction")) {
            return;
        }
        if (ic_stop_completing(cenv)) {
            return;
        }
        if (++added >= kMaxSpellMatches) {
            return;
        }
    }
}

static std::vector<std::string> tokenize_command_line(const std::string& line) {
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

static std::string unquote_path(const std::string& path) {
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
        if (c == ' ' || c == '\t' || c == '\'' || c == '"' || c == '\\' || c == '(' || c == ')' ||
            c == '[' || c == ']' || c == '{' || c == '}' || c == '&' || c == '|' || c == ';' ||
            c == '<' || c == '>' || c == '*' || c == '?' || c == '$' || c == '`') {
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

static bool is_interactive_builtin(const std::string& cmd) {
    static const std::unordered_set<std::string> script_only_builtins = {
        "break", "continue", "return", "__INTERNAL_SUBSHELL__", "local",      "shift", "if",
        "[[",    "[",        ":",      "login-startup-arg",     "prompt_test"};

    return script_only_builtins.find(cmd) == script_only_builtins.end();
}

static CompletionContext detect_completion_context(const char* prefix) {
    std::string prefix_str(prefix);

    if (prefix_str.find('/') == 0 || prefix_str.find("./") == 0 || prefix_str.find("../") == 0) {
        return CONTEXT_PATH;
    }

    std::vector<std::string> tokens = tokenize_command_line(prefix_str);

    if (tokens.size() > 1) {
        return CONTEXT_ARGUMENT;
    }

    size_t last_unquoted_space = find_last_unquoted_space(prefix_str);
    if (last_unquoted_space != std::string::npos) {
        return CONTEXT_ARGUMENT;
    }
    return CONTEXT_COMMAND;
}

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix) {
    if (ic_stop_completing(cenv))
        return;

    if (completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    size_t prefix_len = prefix_str.length();

    std::vector<std::string> builtin_cmds;
    std::vector<std::string> function_names;
    std::unordered_set<std::string> aliases;
    std::vector<std::filesystem::path> cached_executables;

    if (g_shell && (g_shell->get_built_ins() != nullptr)) {
        builtin_cmds = g_shell->get_built_ins()->get_builtin_commands();
    }

    if (g_shell && (g_shell->get_shell_script_interpreter() != nullptr)) {
        function_names = g_shell->get_shell_script_interpreter()->get_function_names();
    }

    if (g_shell) {
        auto shell_aliases = g_shell->get_aliases();
        for (const auto& alias : shell_aliases) {
            aliases.insert(alias.first);
        }
    }

    cached_executables = cjsh_filesystem::read_cached_executables();

    auto builtin_filter = [&](const std::string& cmd) { return is_interactive_builtin(cmd); };

    process_command_candidates(
        cenv, builtin_cmds, prefix_str, prefix_len, "builtin", "builtin commands",
        [](const std::string& value) { return value; }, builtin_filter);
    if (completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, function_names, prefix_str, prefix_len, "function",
                               "function commands", [](const std::string& value) { return value; });
    if (completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, aliases, prefix_str, prefix_len, "alias", "aliases",
                               [](const std::string& value) { return value; });
    if (completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(
        cenv, cached_executables, prefix_str, prefix_len, "system", "cached executables",
        [](const std::filesystem::path& value) { return value.filename().string(); });

    if (!ic_has_completions(cenv) && g_completion_spell_correction_enabled) {
        std::string normalized_prefix = normalize_for_comparison(prefix_str);
        if (should_consider_spell_correction(normalized_prefix)) {
            std::unordered_map<std::string, SpellCorrectionMatch> spell_matches;

            collect_spell_correction_candidates(
                builtin_cmds, [](const std::string& value) { return value; }, builtin_filter,
                normalized_prefix, spell_matches);

            collect_spell_correction_candidates(
                function_names, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            collect_spell_correction_candidates(
                aliases, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            collect_spell_correction_candidates(
                cached_executables,
                [](const std::filesystem::path& value) { return value.filename().string(); },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            if (!spell_matches.empty()) {
                add_spell_correction_matches(cenv, spell_matches, prefix_len);
            }
        }
    }
}

static bool looks_like_file_path(const std::string& str) {
    if (str.empty())
        return false;

    if (str[0] == '/' || str.rfind("./", 0) == 0 || str.rfind("../", 0) == 0 ||
        str.rfind("~/", 0) == 0 || str.find('/') != std::string::npos) {
        return true;
    }

    size_t dot_pos = str.rfind('.');
    if (dot_pos != std::string::npos && dot_pos > 0 && dot_pos < str.length() - 1) {
        std::string extension = str.substr(dot_pos + 1);

        static const std::unordered_set<std::string> file_extensions = {
            "txt",  "log",  "conf", "config", "json", "xml",  "yaml", "yml", "cpp",
            "c",    "h",    "hpp",  "py",     "js",   "ts",   "java", "sh",  "bash",
            "md",   "html", "css",  "sql",    "tar",  "gz",   "zip",  "pdf", "doc",
            "docx", "xls",  "xlsx", "png",    "jpg",  "jpeg", "gif",  "mp3", "mp4"};

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
        } else if (matches_completion_prefix(line, prefix_str) && line != prefix_str) {
            should_match = true;
        }

        if (should_match) {
            auto freq_it = g_completion_frequency.find(line);
            int frequency = (freq_it != g_completion_frequency.end()) ? freq_it->second : 1;
            matches.emplace_back(std::move(line), frequency);
        }

        line.clear();
    }

    std::sort(matches.begin(), matches.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    const size_t max_suggestions = 15;
    size_t count = 0;

    for (const auto& match : matches) {
        if (completion_limit_hit_with_log("history suggestions"))
            return;

        const std::string& completion = match.first;
        long delete_before = static_cast<long>(prefix_len);

        if (!safe_add_completion_prim_with_source(cenv, completion.c_str(), nullptr, nullptr,
                                                  "history", delete_before, 0))
            return;
        if (++count >= max_suggestions || ic_stop_completing(cenv))
            return;
    }
}

static bool should_complete_directories_only(const std::string& prefix) {
    std::string command;
    size_t first_space = prefix.find(' ');

    if (first_space != std::string::npos) {
        command = prefix.substr(0, first_space);
    } else {
        return false;
    }

    static const std::unordered_set<std::string> directory_only_commands = {"cd", "ls", "dir",
                                                                            "rmdir"};
    if (g_completion_case_sensitive) {
        return directory_only_commands.find(command) != directory_only_commands.end();
    }

    std::string lowered_command = command;
    std::transform(lowered_command.begin(), lowered_command.end(), lowered_command.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return directory_only_commands.find(lowered_command) != directory_only_commands.end();
}

bool starts_with_case_insensitive(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) {
        return false;
    }

    return std::equal(prefix.begin(), prefix.end(), str.begin(),
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

static bool starts_with_case_sensitive(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) {
        return false;
    }

    return std::equal(prefix.begin(), prefix.end(), str.begin());
}

static bool matches_completion_prefix(const std::string& str, const std::string& prefix) {
    if (g_completion_case_sensitive) {
        return starts_with_case_sensitive(str, prefix);
    }

    return starts_with_case_insensitive(str, prefix);
}

static bool equals_completion_token(const std::string& value, const std::string& target) {
    if (g_completion_case_sensitive) {
        return value == target;
    }

    if (value.length() != target.length()) {
        return false;
    }

    return std::equal(value.begin(), value.end(), target.begin(),
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

static bool starts_with_token(const std::string& value, const std::string& target_prefix) {
    if (g_completion_case_sensitive) {
        return starts_with_case_sensitive(value, target_prefix);
    }

    return starts_with_case_insensitive(value, target_prefix);
}

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
    if (ic_stop_completing(cenv))
        return;

    if (completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    bool directories_only = should_complete_directories_only(prefix_str);

    size_t last_space = find_last_unquoted_space(prefix_str);

    bool has_tilde = false;
    bool has_dash = false;
    std::string prefix_before;
    std::string special_part;

    if (last_space != std::string::npos) {
        prefix_before = prefix_str.substr(0, last_space + 1);

        if (last_space + 1 < prefix_str.length()) {
            special_part = prefix_str.substr(last_space + 1);

            if (!special_part.empty() && special_part[0] == '~') {
                has_tilde = true;
            } else if (!special_part.empty() && special_part[0] == '-' &&
                       (special_part.length() == 1 || special_part[1] == '/')) {
                has_dash = true;
            }
        } else {
            special_part.clear();
        }
    } else if (!prefix_str.empty() && prefix_str[0] == '~') {
        has_tilde = true;
        special_part = prefix_str;
    } else if (!prefix_str.empty() && prefix_str[0] == '-' &&
               (prefix_str.length() == 1 || prefix_str[1] == '/')) {
        has_dash = true;
        special_part = prefix_str;
    }

    if (has_tilde && (special_part.length() == 1 || special_part[1] == '/')) {
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
        bool treat_as_directory = !unquoted_special.empty() && unquoted_special.back() == '/';
        determine_directory_target(dir_to_complete, treat_as_directory, dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, false, 30, false,
                                               "tilde"))
                    return;
            }
        } catch (const std::exception& e) {
        }

        return;
    }
    if (has_dash && (special_part.length() == 1 || special_part[1] == '/')) {
        std::string unquoted_special = unquote_path(special_part);
        std::string path_after_dash =
            unquoted_special.length() > 1 ? unquoted_special.substr(2) : "";
        std::string dir_to_complete = g_shell->get_previous_directory();

        if (dir_to_complete.empty()) {
            return;
        }

        if (unquoted_special.length() > 1) {
            dir_to_complete += "/" + path_after_dash;
        }

        namespace fs = std::filesystem;
        fs::path dir_path;
        std::string match_prefix;
        bool treat_as_directory = !unquoted_special.empty() && unquoted_special.back() == '/';
        determine_directory_target(dir_to_complete, treat_as_directory, dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, false, 30, false,
                                               "dash"))
                    return;
            }
        } catch (const std::exception& e) {
        }

        return;
    }

    if (!prefix_before.empty()) {
        std::string command_part = prefix_before;

        while (!command_part.empty() &&
               (command_part.back() == ' ' || command_part.back() == '\t')) {
            command_part.pop_back();
        }

        if (equals_completion_token(command_part, "cd") || starts_with_token(command_part, "cd ")) {
            if (!config::smart_cd_enabled) {
            } else {
                if (g_shell && (g_shell->get_built_ins() != nullptr)) {
                    const auto& bookmarks = g_shell->get_built_ins()->get_directory_bookmarks();
                    std::string bookmark_match_prefix = unquote_path(special_part);

                    for (const auto& bookmark : bookmarks) {
                        const std::string& bookmark_name = bookmark.first;
                        const std::string& bookmark_path = bookmark.second;

                        if (bookmark_match_prefix.empty() ||
                            matches_completion_prefix(bookmark_name, bookmark_match_prefix)) {
                            namespace fs = std::filesystem;
                            if (fs::exists(bookmark_path) && fs::is_directory(bookmark_path)) {
                                std::string current_dir_item = "./" + bookmark_name;
                                if (fs::exists(current_dir_item) &&
                                    fs::is_directory(current_dir_item)) {
                                    continue;
                                }

                                long delete_before = static_cast<long>(special_part.length());

                                std::string completion_text = bookmark_name;

                                if (!safe_add_completion_prim_with_source(
                                        cenv, completion_text.c_str(), nullptr, nullptr, "bookmark",
                                        delete_before, 0))
                                    return;
                            }
                        }
                    }
                }
            }
        }
    }

    const bool has_command_prefix = !prefix_before.empty();
    std::string raw_path_input = has_command_prefix ? special_part : prefix_str;
    std::string path_to_check = unquote_path(raw_path_input);

    if (!ic_stop_completing(cenv) && !path_to_check.empty() && path_to_check.back() == '/') {
        namespace fs = std::filesystem;
        fs::path dir_path(path_to_check);
        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, "", directories_only, 30, false,
                                               "all files"))
                    return;
            }
        } catch (const std::exception& e) {
        }
        return;
    }

    if (directories_only) {
        std::string path_to_complete = unquote_path(raw_path_input);

        namespace fs = std::filesystem;
        fs::path dir_path;
        std::string match_prefix;
        bool treat_as_directory = path_to_complete.empty() || path_to_complete.back() == '/';
        determine_directory_target(path_to_complete, treat_as_directory, dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, true, 30, true,
                                               "directory-only"))
                    return;
            }
        } catch (const std::exception& e) {
        }
    } else {
        std::string path_to_complete = unquote_path(raw_path_input);

        namespace fs = std::filesystem;
        fs::path dir_path;
        std::string match_prefix;
        bool treat_as_directory = path_to_complete.empty() || path_to_complete.back() == '/';
        determine_directory_target(path_to_complete, treat_as_directory, dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, false, 30, true,
                                               "general filename"))
                    return;
            }
        } catch (const std::exception& e) {
        }
    }
}

void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix) {
    if (ic_stop_completing(cenv))
        return;

    const char* effective_prefix = (prefix != nullptr) ? prefix : "";
    const char* current_line_prefix = extract_current_line_prefix(effective_prefix);

    completion_session_begin(cenv, effective_prefix);

    if (current_line_prefix[0] == '\0') {
        cjsh_history_completer(cenv, current_line_prefix);
        completion_session_end();
        return;
    }

    CompletionContext context = detect_completion_context(current_line_prefix);

    switch (context) {
        case CONTEXT_COMMAND:
            cjsh_history_completer(cenv, current_line_prefix);
            if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
                completion_session_end();
                return;
            }

            cjsh_command_completer(cenv, current_line_prefix);
            if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
                completion_session_end();
                return;
            }

            cjsh_filename_completer(cenv, current_line_prefix);
            break;

        case CONTEXT_PATH:
            cjsh_history_completer(cenv, current_line_prefix);
            cjsh_filename_completer(cenv, current_line_prefix);
            break;

        case CONTEXT_ARGUMENT: {
            std::string prefix_str(current_line_prefix);
            std::vector<std::string> tokens = tokenize_command_line(prefix_str);

            if (!tokens.empty() && equals_completion_token(tokens[0], "cd")) {
                cjsh_filename_completer(cenv, current_line_prefix);
            } else {
                cjsh_history_completer(cenv, current_line_prefix);
                cjsh_filename_completer(cenv, current_line_prefix);
            }
            break;
        }
    }

    completion_session_end();
}

void initialize_completion_system() {
    if (config::completions_enabled) {
        ic_set_default_completer(cjsh_default_completer, nullptr);
    } else {
        ic_set_default_completer(nullptr, nullptr);
        ic_enable_completion_preview(false);
        ic_enable_hint(false);
        ic_enable_auto_tab(false);
        ic_enable_inline_help(false);
    }
    if (!enforce_history_limit_internal(nullptr)) {
        std::cerr << "cjsh: warning: failed to enforce history limit; history file may exceed the "
                     "configured size."
                  << '\n';
    }
}

void update_completion_frequency(const std::string& command) {
    if (!command.empty()) {
        g_completion_frequency[command]++;
    }
}

void cleanup_completion_system() {
    if (g_current_completion_tracker != nullptr) {
        delete g_current_completion_tracker;
        g_current_completion_tracker = nullptr;
    }
}

void set_completion_case_sensitive(bool case_sensitive) {
    g_completion_case_sensitive = case_sensitive;
}

bool is_completion_case_sensitive() {
    return g_completion_case_sensitive;
}

void set_completion_spell_correction_enabled(bool enabled) {
    g_completion_spell_correction_enabled = enabled;
    ic_enable_spell_correct(enabled);
}

bool is_completion_spell_correction_enabled() {
    return g_completion_spell_correction_enabled;
}

bool set_history_max_entries(long max_entries, std::string* error_message) {
    long resolved = max_entries;
    if (max_entries < 0) {
        if (max_entries == -1) {
            resolved = kHistoryDefaultEntries;
        } else {
            if (error_message != nullptr) {
                *error_message = "History limit must be zero or greater.";
            }
            return false;
        }
    }

    if (resolved > kHistoryAbsoluteMaxEntries) {
        if (error_message != nullptr) {
            *error_message =
                "History limit cannot exceed " + std::to_string(kHistoryAbsoluteMaxEntries) + ".";
        }
        return false;
    }

    long previous_limit = g_history_max_entries_value;
    g_history_max_entries_value = resolved;

    if (!enforce_history_limit_internal(error_message)) {
        g_history_max_entries_value = previous_limit;
        enforce_history_limit_internal(nullptr);
        return false;
    }

    return true;
}

long get_history_max_entries() {
    return g_history_max_entries_value;
}

long get_history_default_history_limit() {
    return kHistoryDefaultEntries;
}

long get_history_min_history_limit() {
    return kHistoryMinEntries;
}

long get_history_max_history_limit() {
    return kHistoryAbsoluteMaxEntries;
}