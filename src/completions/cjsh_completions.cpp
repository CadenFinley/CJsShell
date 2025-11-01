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
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "builtin.h"
#include "builtins_completions_handler.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "completion_history.h"
#include "completion_spell.h"
#include "completion_tracker.h"
#include "completion_utils.h"
#include "external_sub_completions.h"
#include "isocline.h"
#include "shell.h"
#include "shell_script_interpreter.h"

std::map<std::string, int> g_completion_frequency;
bool g_completion_case_sensitive = false;
bool g_completion_spell_correction_enabled = true;

enum CompletionContext : std::uint8_t {
    CONTEXT_COMMAND,
    CONTEXT_ARGUMENT,
    CONTEXT_PATH
};

namespace {

const char* classify_entry_source(const std::filesystem::directory_entry& entry);

const char* extract_current_line_prefix(const char* prefix) {
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

bool add_command_completion(ic_completion_env_t* cenv, const std::string& candidate,
                            size_t prefix_len, const char* source, const char* debug_label) {
    (void)debug_label;
    long delete_before = static_cast<long>(prefix_len);
    std::string completion_text = candidate;
    if (completion_text.empty() || completion_text.back() != ' ')
        completion_text.push_back(' ');
    return completion_tracker::safe_add_completion_prim_with_source(
        cenv, completion_text.c_str(), nullptr, nullptr, source, delete_before, 0);
}

std::string build_completion_suffix(const std::filesystem::directory_entry& entry) {
    std::string completion_suffix =
        completion_utils::quote_path_if_needed(entry.path().filename().string());
    if (entry.is_directory())
        completion_suffix += "/";
    else
        completion_suffix += " ";
    return completion_suffix;
}

bool add_path_completion(ic_completion_env_t* cenv, const std::filesystem::directory_entry& entry,
                         long delete_before, const std::string& completion_suffix) {
    const char* source = classify_entry_source(entry);
    if (delete_before == 0)
        return completion_tracker::safe_add_completion_with_source(cenv, completion_suffix.c_str(),
                                                                   source);
    return completion_tracker::safe_add_completion_prim_with_source(
        cenv, completion_suffix.c_str(), nullptr, nullptr, source, delete_before, 0);
}

void determine_directory_target(const std::string& path, bool treat_as_directory,
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

bool has_shebang_line(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;
    char prefix[2];
    file.read(prefix, sizeof(prefix));
    return file.gcount() == static_cast<std::streamsize>(sizeof(prefix)) && prefix[0] == '#' &&
           prefix[1] == '!';
}

const char* classify_entry_source(const std::filesystem::directory_entry& entry) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (entry.is_directory(ec))
        return "directory";
    if (ec)
        return "file";

    ec.clear();
    if (!entry.is_regular_file(ec) || ec)
        return "file";

    ec.clear();
    fs::file_status status = entry.status(ec);
    if (ec)
        return "file";

    constexpr auto exec_mask =
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
    if ((status.permissions() & exec_mask) == fs::perms::none)
        return "file";

    return has_shebang_line(entry.path()) ? "executable script" : "executable binary";
}

template <typename Container, typename Extractor>
void process_command_candidates(
    ic_completion_env_t* cenv, const Container& container, const std::string& prefix,
    size_t prefix_len, const char* source, const char* debug_label, Extractor extractor,
    const std::function<bool(const std::string&)>& filter = {},
    const std::function<std::string(const std::string&)>& source_provider = {}) {
    for (const auto& item : container) {
        if (completion_tracker::completion_limit_hit_with_log(debug_label))
            return;
        if (ic_stop_completing(cenv))
            return;
        std::string candidate = extractor(item);
        if (filter && !filter(candidate))
            continue;
        if (!completion_utils::matches_completion_prefix(candidate, prefix))
            continue;
        const char* source_ptr = source;
        std::string dynamic_source;
        if (source_provider) {
            dynamic_source = source_provider(candidate);
            if (!dynamic_source.empty())
                source_ptr = dynamic_source.c_str();
        }
        if (!add_command_completion(cenv, candidate, prefix_len, source_ptr, debug_label))
            return;
        if (ic_stop_completing(cenv))
            return;
    }
}

bool iterate_directory_entries(ic_completion_env_t* cenv, const std::filesystem::path& dir_path,
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
        if (completion_tracker::completion_limit_hit_with_log(limit_label.c_str()))
            return false;
        if (directories_only && !entry.is_directory())
            continue;
        std::string filename = entry.path().filename().string();
        if (filename.empty())
            continue;
        if (skip_hidden_without_prefix && match_prefix.empty() && filename[0] == '.')
            continue;
        if (!match_prefix.empty() &&
            !completion_utils::matches_completion_prefix(filename, match_prefix))
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

bool is_interactive_builtin(const std::string& cmd) {
    static const std::unordered_set<std::string> script_only_builtins = {"__INTERNAL_SUBSHELL__",
                                                                         "login-startup-arg"};

    return script_only_builtins.find(cmd) == script_only_builtins.end();
}

CompletionContext detect_completion_context(const char* prefix) {
    std::string prefix_str(prefix);

    if (prefix_str.find('/') == 0 || prefix_str.find("./") == 0 || prefix_str.find("../") == 0) {
        return CONTEXT_PATH;
    }

    std::vector<std::string> tokens = completion_utils::tokenize_command_line(prefix_str);

    if (tokens.size() > 1) {
        return CONTEXT_ARGUMENT;
    }

    size_t last_unquoted_space = completion_utils::find_last_unquoted_space(prefix_str);
    if (last_unquoted_space != std::string::npos) {
        return CONTEXT_ARGUMENT;
    }
    return CONTEXT_COMMAND;
}

}  // namespace

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix) {
    if (ic_stop_completing(cenv))
        return;

    if (completion_tracker::completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    size_t prefix_len = prefix_str.length();

    std::vector<std::string> builtin_cmds;
    std::vector<std::string> function_names;
    std::vector<std::string> alias_names;
    std::vector<std::string> abbreviation_names;
    const std::unordered_map<std::string, std::string>* alias_map = nullptr;
    const std::unordered_map<std::string, std::string>* abbreviation_map = nullptr;
    std::vector<std::string> executables_in_path;

    if (g_shell && (g_shell->get_built_ins() != nullptr)) {
        builtin_cmds = g_shell->get_built_ins()->get_builtin_commands();
    }

    if (g_shell && (g_shell->get_shell_script_interpreter() != nullptr)) {
        function_names = g_shell->get_shell_script_interpreter()->get_function_names();
    }

    if (g_shell) {
        alias_map = &g_shell->get_aliases();
        alias_names.reserve(alias_map->size());
        for (const auto& alias : *alias_map) {
            alias_names.push_back(alias.first);
        }

        abbreviation_map = &g_shell->get_abbreviations();
        abbreviation_names.reserve(abbreviation_map->size());
        for (const auto& entry : *abbreviation_map) {
            abbreviation_names.push_back(entry.first);
        }
    }

    executables_in_path = cjsh_filesystem::get_executables_in_path();

    auto builtin_filter = [&](const std::string& cmd) { return is_interactive_builtin(cmd); };

    auto builtin_summary_provider = [](const std::string& cmd) -> std::string {
        return builtin_completions::get_builtin_summary(cmd);
    };

    process_command_candidates(
        cenv, builtin_cmds, prefix_str, prefix_len, "builtin", "builtin commands",
        [](const std::string& value) { return value; }, builtin_filter, builtin_summary_provider);
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, function_names, prefix_str, prefix_len, "function",
                               "function commands", [](const std::string& value) { return value; });
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    auto alias_source_provider = [alias_map](const std::string& name) -> std::string {
        if (alias_map == nullptr)
            return {};
        auto it = alias_map->find(name);
        if (it == alias_map->end())
            return {};
        return it->second;
    };

    process_command_candidates(
        cenv, alias_names, prefix_str, prefix_len, "alias", "aliases",
        [](const std::string& value) { return value; }, std::function<bool(const std::string&)>{},
        alias_source_provider);
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    auto abbreviation_source_provider = [abbreviation_map](const std::string& name) -> std::string {
        if (abbreviation_map == nullptr)
            return {};
        auto it = abbreviation_map->find(name);
        if (it == abbreviation_map->end())
            return {};
        return it->second;
    };

    process_command_candidates(
        cenv, abbreviation_names, prefix_str, prefix_len, "abbreviation", "abbreviations",
        [](const std::string& value) { return value; }, std::function<bool(const std::string&)>{},
        abbreviation_source_provider);
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    size_t summary_fetch_budget = prefix_len == 0 ? 2 : 5;
    auto system_summary_provider = [&](const std::string& cmd) -> std::string {
        std::string summary = get_command_summary(cmd, false);
        if (!summary.empty())
            return summary;
        if (summary_fetch_budget == 0)
            return {};
        --summary_fetch_budget;
        return get_command_summary(cmd, true);
    };

    process_command_candidates(
        cenv, executables_in_path, prefix_str, prefix_len, "system installed command",
        "executables in PATH", [](const std::string& value) { return value; }, {},
        system_summary_provider);

    if (!ic_has_completions(cenv) && g_completion_spell_correction_enabled) {
        std::string normalized_prefix = completion_utils::normalize_for_comparison(prefix_str);
        if (completion_spell::should_consider_spell_correction(normalized_prefix)) {
            std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> spell_matches;

            completion_spell::collect_spell_correction_candidates(
                builtin_cmds, [](const std::string& value) { return value; }, builtin_filter,
                normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                function_names, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                alias_names, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                abbreviation_names, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                executables_in_path, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            if (!spell_matches.empty()) {
                completion_spell::add_spell_correction_matches(cenv, spell_matches, prefix_len);
            }
        }
    }
}

bool looks_like_file_path(const std::string& str) {
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

    if (completion_tracker::completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    size_t prefix_len = prefix_str.length();

    std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path);
    if (!history_file.is_open()) {
        return;
    }

    struct HistoryMatch {
        std::string command;
        int frequency;
        bool has_exit_code;
        int exit_code;
    };

    std::vector<HistoryMatch> matches;
    matches.reserve(50);

    std::string line;
    line.reserve(256);

    int last_exit_code = 0;
    bool has_last_exit_code = false;

    while (std::getline(history_file, line) && matches.size() < 50) {
        if (line.empty())
            continue;

        if (!line.empty() && line[0] == '#') {
            last_exit_code = 0;
            has_last_exit_code = false;

            const char* cursor = line.c_str() + 1;
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;

            if (*cursor != '\0') {
                char* endptr = nullptr;
                (void)std::strtoll(cursor, &endptr, 10);
                cursor = endptr;

                while (*cursor == ' ' || *cursor == '\t')
                    ++cursor;

                if (*cursor != '\0' && *cursor != '\n' && *cursor != '\r') {
                    long exit_ll = std::strtol(cursor, &endptr, 10);
                    if (endptr != cursor) {
                        last_exit_code = static_cast<int>(exit_ll);
                        has_last_exit_code = (last_exit_code != IC_HISTORY_EXIT_CODE_UNKNOWN);
                    }
                }
            }

            continue;
        }

        if (looks_like_file_path(line)) {
            last_exit_code = 0;
            has_last_exit_code = false;
            continue;
        }

        bool should_match = false;
        if (prefix_len == 0) {
            should_match = (line != prefix_str);
        } else if (completion_utils::matches_completion_prefix(line, prefix_str) &&
                   line != prefix_str) {
            should_match = true;
        }

        if (should_match) {
            auto freq_it = g_completion_frequency.find(line);
            int frequency = (freq_it != g_completion_frequency.end()) ? freq_it->second : 1;
            matches.push_back(
                HistoryMatch{std::move(line), frequency, has_last_exit_code, last_exit_code});
        }

        last_exit_code = 0;
        has_last_exit_code = false;

        line.clear();
    }

    std::sort(matches.begin(), matches.end(), [](const HistoryMatch& a, const HistoryMatch& b) {
        return a.frequency > b.frequency;
    });
    const size_t max_suggestions = 15;
    size_t count = 0;

    for (const auto& match : matches) {
        if (completion_tracker::completion_limit_hit_with_log("history suggestions"))
            return;

        const std::string& completion = match.command;
        long delete_before = static_cast<long>(prefix_len);

        const bool display_exit_code =
            match.has_exit_code && match.exit_code != IC_HISTORY_EXIT_CODE_UNKNOWN;
        std::string source_label =
            display_exit_code ? "history: " + std::to_string(match.exit_code) : "history";
        if (!completion_tracker::safe_add_completion_prim_with_source(
                cenv, completion.c_str(), nullptr, nullptr, source_label.c_str(), delete_before, 0))
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

void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
    if (ic_stop_completing(cenv))
        return;

    if (completion_tracker::completion_limit_hit()) {
        return;
    }

    std::string prefix_str(prefix);
    bool directories_only = should_complete_directories_only(prefix_str);

    size_t last_space = completion_utils::find_last_unquoted_space(prefix_str);

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
        std::string unquoted_special = completion_utils::unquote_path(special_part);
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
        std::string unquoted_special = completion_utils::unquote_path(special_part);
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

        if (completion_utils::equals_completion_token(command_part, "cd") ||
            completion_utils::starts_with_token(command_part, "cd ")) {
            if (!config::smart_cd_enabled) {
            } else {
                if (g_shell && (g_shell->get_built_ins() != nullptr)) {
                    const auto& bookmarks = g_shell->get_built_ins()->get_directory_bookmarks();
                    std::string bookmark_match_prefix =
                        completion_utils::unquote_path(special_part);

                    for (const auto& bookmark : bookmarks) {
                        const std::string& bookmark_name = bookmark.first;
                        const std::string& bookmark_path = bookmark.second;

                        if (bookmark_match_prefix.empty() ||
                            completion_utils::matches_completion_prefix(bookmark_name,
                                                                        bookmark_match_prefix)) {
                            namespace fs = std::filesystem;
                            if (fs::exists(bookmark_path) && fs::is_directory(bookmark_path)) {
                                std::string current_dir_item = "./" + bookmark_name;
                                if (fs::exists(current_dir_item) &&
                                    fs::is_directory(current_dir_item)) {
                                    continue;
                                }

                                long delete_before = static_cast<long>(special_part.length());

                                std::string completion_text = bookmark_name;

                                if (!completion_tracker::safe_add_completion_prim_with_source(
                                        cenv, completion_text.c_str(), nullptr, nullptr,
                                        bookmark_path.c_str(), delete_before, 0))
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
    std::string path_to_check = completion_utils::unquote_path(raw_path_input);

    if (!ic_stop_completing(cenv) && !path_to_check.empty() && path_to_check.back() == '/') {
        namespace fs = std::filesystem;
        fs::path dir_path(path_to_check);
        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                bool had_completions_before = ic_has_completions(cenv);
                if (!iterate_directory_entries(cenv, dir_path, "", directories_only, 30, false,
                                               "all files"))
                    return;

                if (directories_only && !ic_has_completions(cenv) && !had_completions_before) {
                    if (!iterate_directory_entries(cenv, dir_path, "", false, 30, false,
                                                   "all files (fallback)"))
                        return;
                }
            }
        } catch (const std::exception& e) {
        }
        return;
    }

    if (directories_only) {
        std::string path_to_complete = completion_utils::unquote_path(raw_path_input);

        namespace fs = std::filesystem;
        fs::path dir_path;
        std::string match_prefix;
        bool treat_as_directory = path_to_complete.empty() || path_to_complete.back() == '/';
        determine_directory_target(path_to_complete, treat_as_directory, dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                bool had_completions_before = ic_has_completions(cenv);
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, true, 30, true,
                                               "directory-only"))
                    return;

                if (!ic_has_completions(cenv) && !had_completions_before && !match_prefix.empty()) {
                    if (!iterate_directory_entries(cenv, dir_path, "", true, 30, true,
                                                   "directory-only (all)"))
                        return;
                }

                if (!ic_has_completions(cenv) && !had_completions_before) {
                    if (!iterate_directory_entries(cenv, dir_path, "", false, 30, true,
                                                   "all files (fallback)"))
                        return;
                }
            }
        } catch (const std::exception& e) {
        }
    } else {
        std::string path_to_complete = completion_utils::unquote_path(raw_path_input);

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

    completion_tracker::completion_session_begin(cenv, effective_prefix);

    CompletionContext context;
    if (current_line_prefix[0] == '\0') {
        context = CONTEXT_COMMAND;
    } else {
        context = detect_completion_context(current_line_prefix);
    }

    switch (context) {
        case CONTEXT_COMMAND:
            cjsh_command_completer(cenv, current_line_prefix);
            if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
                completion_tracker::completion_session_end();
                return;
            }

            cjsh_history_completer(cenv, current_line_prefix);
            if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
                completion_tracker::completion_session_end();
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
            std::vector<std::string> tokens = completion_utils::tokenize_command_line(prefix_str);

            bool ends_with_space =
                !prefix_str.empty() && std::isspace(static_cast<unsigned char>(prefix_str.back()));

            if (!tokens.empty()) {
                std::vector<std::string> args;
                if (tokens.size() > 1) {
                    args.assign(tokens.begin() + 1, tokens.end());
                }
                if (ends_with_space) {
                    args.emplace_back("");
                }

                handle_external_sub_completions(cenv, current_line_prefix);
            }

            if (!tokens.empty() && completion_utils::equals_completion_token(tokens[0], "cd")) {
                cjsh_filename_completer(cenv, current_line_prefix);
            } else {
                cjsh_history_completer(cenv, current_line_prefix);
                cjsh_filename_completer(cenv, current_line_prefix);
            }
            break;
        }
    }

    completion_tracker::completion_session_end();
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
    if (!completion_history::enforce_history_limit(nullptr)) {
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
    completion_tracker::completion_session_end();
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
    return completion_history::set_history_max_entries(max_entries, error_message);
}

long get_history_max_entries() {
    return completion_history::get_history_max_entries();
}

long get_history_default_history_limit() {
    return completion_history::get_history_default_history_limit();
}

long get_history_min_history_limit() {
    return completion_history::get_history_min_history_limit();
}

long get_history_max_history_limit() {
    return completion_history::get_history_max_history_limit();
}
