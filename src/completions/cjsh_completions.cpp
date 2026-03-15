/*
  cjsh_completions.cpp

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

#include "cjsh_completions.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
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
#include "error_out.h"
#include "external_sub_completions.h"
#include "interpreter.h"
#include "isocline.h"
#include "job_control.h"
#include "quote_state.h"
#include "shell.h"
#include "shell_env.h"
#include "token_constants.h"

namespace {
bool g_completion_case_sensitive = false;
bool g_completion_spell_correction_enabled = true;
}  // namespace

enum CompletionContext : std::uint8_t {
    CONTEXT_COMMAND,
    CONTEXT_ARGUMENT,
    CONTEXT_PATH
};

namespace {

const char* classify_entry_source(const std::filesystem::directory_entry& entry);

const std::vector<std::string>& control_structure_keywords() {
    static const std::vector<std::string> keywords = {"if",    "then", "elif", "else",    "fi",
                                                      "case",  "esac", "for",  "select",  "while",
                                                      "until", "do",   "done", "function"};
    return keywords;
}

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

bool prepare_prefix_state(ic_completion_env_t* cenv, const char* prefix, std::string& prefix_str,
                          size_t& prefix_len) {
    if (ic_stop_completing(cenv))
        return false;
    if (completion_tracker::completion_limit_hit())
        return false;

    prefix_str = prefix ? prefix : "";
    prefix_len = prefix_str.length();
    return true;
}

int from_hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

bool decode_history_command_line(const std::string& raw, std::string& decoded) {
    decoded.clear();
    decoded.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        char ch = raw[i];
        if (ch != '\\') {
            decoded.push_back(ch);
            continue;
        }
        if (i + 1 >= raw.size()) {
            return false;
        }
        char esc = raw[++i];
        switch (esc) {
            case 'n':
                decoded.push_back('\n');
                break;
            case 't':
                decoded.push_back('\t');
                break;
            case 'r':
                break;
            case '\\':
                decoded.push_back('\\');
                break;
            case 'x': {
                if (i + 2 >= raw.size()) {
                    return false;
                }
                char h1 = raw[i + 1];
                char h2 = raw[i + 2];
                if (!std::isxdigit(static_cast<unsigned char>(h1)) ||
                    !std::isxdigit(static_cast<unsigned char>(h2))) {
                    return false;
                }
                int hi = from_hex_digit(h1);
                int lo = from_hex_digit(h2);
                if (hi < 0 || lo < 0) {
                    return false;
                }
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

bool add_command_completion(ic_completion_env_t* cenv, const std::string& candidate,
                            size_t prefix_len, const char* source) {
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

bool is_runnable_file_entry(const std::filesystem::directory_entry& entry) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (entry.is_directory(ec) || ec)
        return false;

    ec.clear();
    if (!entry.is_regular_file(ec) || ec)
        return false;

    ec.clear();
    fs::file_status status = entry.status(ec);
    if (ec)
        return false;

    constexpr auto exec_mask =
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
    if ((status.permissions() & exec_mask) != fs::perms::none)
        return true;

    return has_shebang_line(entry.path());
}

bool is_executable_or_script_entry(const std::filesystem::directory_entry& entry) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (entry.is_directory(ec))
        return true;
    if (ec)
        return false;

    return is_runnable_file_entry(entry);
}

int completion_entry_priority(const std::filesystem::directory_entry& entry) {
    namespace fs = std::filesystem;
    if (is_runnable_file_entry(entry))
        return 0;

    std::error_code ec;
    if (entry.is_directory(ec) && !ec)
        return 1;

    return 2;
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
    size_t prefix_len, const char* source, Extractor extractor,
    const std::function<bool(const std::string&)>& filter = {},
    const std::function<std::string(const std::string&)>& source_provider = {}) {
    for (const auto& item : container) {
        if (completion_tracker::completion_limit_hit())
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
        if (!add_command_completion(cenv, candidate, prefix_len, source_ptr))
            return;
        if (ic_stop_completing(cenv))
            return;
    }
}

bool iterate_directory_entries(
    ic_completion_env_t* cenv, const std::filesystem::path& dir_path,
    const std::string& match_prefix, bool directories_only, bool skip_hidden_without_prefix,
    const char* debug_label,
    const std::function<bool(const std::filesystem::directory_entry&)>& entry_filter = {},
    bool prioritize_runnable_entries = false) {
    namespace fs = std::filesystem;
    std::string limit_label = std::string(debug_label) + " completion";

    std::error_code ec;
    fs::directory_iterator it(dir_path, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        return true;
    }

    auto emit_completion_for_entry = [&](const fs::directory_entry& entry) -> bool {
        if (ic_stop_completing(cenv))
            return false;
        if (completion_tracker::completion_limit_hit())
            return false;

        long delete_before = match_prefix.empty() ? 0 : static_cast<long>(match_prefix.length());
        std::string completion_suffix = build_completion_suffix(entry);
        if (!add_path_completion(cenv, entry, delete_before, completion_suffix))
            return false;
        if (ic_stop_completing(cenv))
            return false;
        return true;
    };

    std::vector<fs::directory_entry> deferred_entries;
    if (prioritize_runnable_entries)
        deferred_entries.reserve(32);

    fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        const auto& entry = *it;
        if (ic_stop_completing(cenv))
            return false;
        if (completion_tracker::completion_limit_hit())
            return false;

        if (directories_only) {
            std::error_code dir_ec;
            if (!entry.is_directory(dir_ec) || dir_ec)
                continue;
        }
        if (entry_filter && !entry_filter(entry))
            continue;

        std::string filename = entry.path().filename().string();
        if (filename.empty())
            continue;
        if (skip_hidden_without_prefix && match_prefix.empty() && filename[0] == '.')
            continue;
        if (!match_prefix.empty() &&
            !completion_utils::matches_completion_prefix(filename, match_prefix))
            continue;

        if (prioritize_runnable_entries) {
            deferred_entries.push_back(entry);
            continue;
        }

        if (!emit_completion_for_entry(entry))
            return false;
    }

    if (!deferred_entries.empty()) {
        auto build_sort_key = [](const fs::directory_entry& entry) {
            std::string name = entry.path().filename().string();
            if (g_completion_case_sensitive)
                return name;
            return completion_utils::normalize_for_comparison(name);
        };

        std::sort(deferred_entries.begin(), deferred_entries.end(),
                  [&](const fs::directory_entry& lhs, const fs::directory_entry& rhs) {
                      int lhs_priority = completion_entry_priority(lhs);
                      int rhs_priority = completion_entry_priority(rhs);
                      if (lhs_priority != rhs_priority)
                          return lhs_priority < rhs_priority;

                      std::string lhs_key = build_sort_key(lhs);
                      std::string rhs_key = build_sort_key(rhs);
                      if (lhs_key == rhs_key) {
                          std::string lhs_name = lhs.path().filename().string();
                          std::string rhs_name = rhs.path().filename().string();
                          return lhs_name < rhs_name;
                      }
                      return lhs_key < rhs_key;
                  });

        for (const auto& entry : deferred_entries) {
            if (!emit_completion_for_entry(entry))
                return false;
        }
    }

    return true;
}

bool is_interactive_builtin(const std::string& cmd) {
    static const std::unordered_set<std::string> script_only_builtins = {"__INTERNAL_SUBSHELL__",
                                                                         "login-startup-arg"};

    return script_only_builtins.find(cmd) == script_only_builtins.end();
}

std::vector<std::string> collect_map_keys(
    const std::unordered_map<std::string, std::string>& values) {
    std::vector<std::string> keys;
    keys.reserve(values.size());
    for (const auto& entry : values) {
        keys.push_back(entry.first);
    }
    return keys;
}

std::function<std::string(const std::string&)> make_map_source_provider(
    const std::unordered_map<std::string, std::string>* values) {
    return [values](const std::string& name) -> std::string {
        if (values == nullptr) {
            return {};
        }
        auto it = values->find(name);
        if (it == values->end()) {
            return {};
        }
        return it->second;
    };
}

std::string builtin_summary_for_command(const std::string& cmd) {
    return builtin_completions::get_builtin_summary(cmd);
}

void add_builtin_command_candidates(ic_completion_env_t* cenv,
                                    const std::vector<std::string>& builtin_cmds,
                                    const std::string& prefix, size_t prefix_len) {
    auto builtin_filter = [](const std::string& cmd) { return is_interactive_builtin(cmd); };
    process_command_candidates(
        cenv, builtin_cmds, prefix, prefix_len, "builtin",
        [](const std::string& value) { return value; }, builtin_filter,
        builtin_summary_for_command);
}

struct CommandCompletionSources {
    std::vector<std::string> builtin_cmds;
    std::vector<std::string> function_names;
    std::vector<std::string> alias_names;
    std::vector<std::string> abbreviation_names;
    std::vector<std::string> executables_in_path;
    const std::unordered_map<std::string, std::string>* alias_map = nullptr;
    const std::unordered_map<std::string, std::string>* abbreviation_map = nullptr;
};

CommandCompletionSources collect_command_completion_sources(bool include_executables) {
    CommandCompletionSources sources;
    if (g_shell && (g_shell->get_built_ins() != nullptr)) {
        sources.builtin_cmds = g_shell->get_built_ins()->get_builtin_commands();
    }

    if (g_shell && (g_shell->get_shell_script_interpreter() != nullptr)) {
        sources.function_names = g_shell->get_shell_script_interpreter()->get_function_names();
    }

    if (g_shell) {
        sources.alias_map = &g_shell->get_aliases();
        sources.alias_names = collect_map_keys(*sources.alias_map);

        sources.abbreviation_map = &g_shell->get_abbreviations();
        sources.abbreviation_names = collect_map_keys(*sources.abbreviation_map);
    }

    if (include_executables) {
        sources.executables_in_path = cjsh_filesystem::get_executables_in_path();
    }

    return sources;
}

bool is_valid_variable_completion_prefix(const std::string& prefix) {
    for (char ch : prefix) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) != 0 || ch == '_' || ch == '?' || ch == '$' || ch == '#' ||
            ch == '*' || ch == '@' || ch == '!') {
            continue;
        }
        return false;
    }
    return true;
}

bool find_last_expandable_dollar(const std::string& token, bool& braced, size_t& var_start) {
    utils::QuoteState quote_state;
    size_t last_dollar = std::string::npos;
    bool last_braced = false;
    size_t last_var_start = std::string::npos;

    for (size_t i = 0; i < token.size(); ++i) {
        char c = token[i];
        if (quote_state.consume_forward(c) == utils::QuoteAdvanceResult::Continue) {
            continue;
        }

        if (c == '$' && !quote_state.in_single_quote) {
            last_dollar = i;
            last_braced = (i + 1 < token.size() && token[i + 1] == '{');
            last_var_start = last_braced ? i + 2 : i + 1;
        }
    }

    if (last_dollar == std::string::npos) {
        return false;
    }

    braced = last_braced;
    var_start = last_var_start;
    return true;
}

bool add_variable_completions(ic_completion_env_t* cenv, const std::string& prefix) {
    if (ic_stop_completing(cenv) || completion_tracker::completion_limit_hit()) {
        return false;
    }

    size_t last_space = completion_utils::find_last_unquoted_space(prefix);
    std::string token_prefix =
        (last_space == std::string::npos) ? prefix : prefix.substr(last_space + 1);

    if (token_prefix.empty()) {
        return false;
    }

    bool braced = false;
    size_t var_start = std::string::npos;
    if (!find_last_expandable_dollar(token_prefix, braced, var_start)) {
        return false;
    }

    if (var_start > token_prefix.size()) {
        return false;
    }

    std::string var_prefix = token_prefix.substr(var_start);
    if (var_prefix.find('}') != std::string::npos) {
        return false;
    }

    if (!is_valid_variable_completion_prefix(var_prefix)) {
        return false;
    }

    std::unordered_set<std::string> candidates;
    if (g_shell && g_shell->get_shell_script_interpreter()) {
        auto names =
            g_shell->get_shell_script_interpreter()->get_variable_manager().get_variable_names();
        candidates.insert(names.begin(), names.end());
    } else {
        const auto& env_vars = cjsh_env::env_vars();
        for (const auto& entry : env_vars) {
            candidates.insert(entry.first);
        }
    }

    static const char* kSpecialVars[] = {"?", "$", "#", "*", "@", "!", "0"};
    for (const char* var_name : kSpecialVars) {
        candidates.insert(var_name);
    }

    if (candidates.empty()) {
        return false;
    }

    std::vector<std::string> ordered_candidates(candidates.begin(), candidates.end());
    auto build_sort_key = [](const std::string& value) {
        return g_completion_case_sensitive ? value
                                           : completion_utils::normalize_for_comparison(value);
    };
    std::sort(ordered_candidates.begin(), ordered_candidates.end(),
              [&](const std::string& lhs, const std::string& rhs) {
                  std::string lhs_key = build_sort_key(lhs);
                  std::string rhs_key = build_sort_key(rhs);
                  if (lhs_key == rhs_key) {
                      return lhs < rhs;
                  }
                  return lhs_key < rhs_key;
              });

    long delete_before = static_cast<long>(var_prefix.size());
    bool added = false;

    for (const auto& name : ordered_candidates) {
        if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv)) {
            break;
        }

        if (!completion_utils::matches_completion_prefix(name, var_prefix)) {
            continue;
        }

        std::string completion_text = name;
        if (braced) {
            completion_text += "}";
        }

        if (!completion_tracker::safe_add_completion_prim_with_source(
                cenv, completion_text.c_str(), nullptr, nullptr, "variable", delete_before, 0)) {
            break;
        }
        added = true;
    }

    return added;
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

bool is_job_control_command(const std::string& token) {
    static const char* kJobCommands[] = {"bg", "fg", "jobs", "jobname", "kill", "disown", "wait"};
    return std::any_of(std::begin(kJobCommands), std::end(kJobCommands),
                       [&](const char* command_name) {
                           return completion_utils::equals_completion_token(token, command_name);
                       });
}

bool add_job_control_argument_completions(ic_completion_env_t* cenv,
                                          const std::vector<std::string>& tokens,
                                          bool ends_with_space) {
    if (tokens.empty())
        return false;

    if (!is_job_control_command(tokens.front()))
        return false;

    if (tokens.size() == 1 && !ends_with_space)
        return false;

    std::string current_prefix;
    if (!ends_with_space && tokens.size() >= 2) {
        current_prefix = tokens.back();
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();
    auto jobs = job_manager.get_all_jobs();
    if (jobs.empty())
        return false;

    long delete_before = static_cast<long>(current_prefix.size());
    bool added = false;

    std::unordered_map<int, std::shared_ptr<JobControlJob>> job_lookup;
    job_lookup.reserve(jobs.size());
    for (const auto& job : jobs) {
        if (job) {
            job_lookup.emplace(job->job_id, job);
        }
    }

    auto build_job_summary = [&](const std::shared_ptr<JobControlJob>& job) {
        const std::string& source = job->has_custom_name() ? job->custom_name : job->command;
        std::string summary = completion_utils::sanitize_job_command_summary(source);
        if (summary.empty()) {
            summary = source.empty() ? std::string("command unavailable") : source;
        }
        return summary;
    };

    auto build_source_label = [&](const std::shared_ptr<JobControlJob>& job,
                                  const std::string& summary_text, const std::string& qualifier) {
        std::string pid_text;
        if (!job->pids.empty()) {
            pid_text = std::to_string(static_cast<long long>(job->pids.front()));
        } else if (job->pgid > 0) {
            pid_text = std::to_string(static_cast<long long>(job->pgid));
        } else {
            pid_text = "unavailable";
        }

        std::string label;
        if (!qualifier.empty()) {
            label.append(qualifier);
            label.append(" · ");
        }
        label.append(summary_text);
        label.append(" · job %");
        label.append(std::to_string(job->job_id));
        label.append(" · pid ");
        label.append(pid_text);
        return label;
    };

    auto matches_prefix = [&](const std::string& candidate) {
        return current_prefix.empty() ||
               completion_utils::matches_completion_prefix(candidate, current_prefix);
    };

    auto add_job_completion = [&](const std::string& token,
                                  const std::shared_ptr<JobControlJob>& job,
                                  const std::string& qualifier) -> bool {
        if (!job)
            return true;
        if (!matches_prefix(token))
            return true;

        std::string insert_text = token;
        if (insert_text.empty() || insert_text.back() != ' ')
            insert_text.push_back(' ');

        std::string summary_text = build_job_summary(job);
        std::string source_label = build_source_label(job, summary_text, qualifier);
        if (!completion_tracker::safe_add_completion_prim_with_source(
                cenv, insert_text.c_str(), nullptr, nullptr, source_label.c_str(), delete_before,
                0)) {
            return false;
        }
        added = true;
        return true;
    };

    auto add_relative_completion = [&](char marker, const std::string& qualifier,
                                       int job_id) -> bool {
        if (job_id < 0)
            return true;
        auto it = job_lookup.find(job_id);
        if (it == job_lookup.end())
            return true;
        std::string token(1, marker);
        return add_job_completion(token, it->second, qualifier);
    };

    if (!add_relative_completion('+', "current job", job_manager.get_current_job()))
        return added;
    if (!add_relative_completion('-', "previous job", job_manager.get_previous_job()))
        return added;

    for (const auto& job : jobs) {
        if (completion_tracker::completion_limit_hit())
            break;
        if (ic_stop_completing(cenv))
            break;

        std::string completion_text = "%" + std::to_string(job->job_id);

        if (!current_prefix.empty() &&
            !completion_utils::matches_completion_prefix(completion_text, current_prefix)) {
            continue;
        }

        if (!add_job_completion(completion_text, job, ""))
            return added;

        if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
            break;
    }

    return added;
}

struct ArgumentCompletionContext {
    std::string current_prefix;
    size_t argument_index;
};

bool build_argument_completion_context(const std::vector<std::string>& tokens, bool ends_with_space,
                                       ArgumentCompletionContext& context) {
    if (tokens.empty()) {
        return false;
    }

    if (ends_with_space) {
        context.current_prefix.clear();
        context.argument_index = tokens.size();
        return true;
    }

    context.current_prefix = tokens.back();
    context.argument_index = tokens.size() - 1;
    return true;
}

bool add_hook_type_completions(ic_completion_env_t* cenv, const std::string& prefix,
                               size_t prefix_len) {
    const auto& descriptors = get_hook_type_descriptors();
    std::vector<std::string> hook_types;
    hook_types.reserve(descriptors.size());
    for (const auto& descriptor : descriptors) {
        if (descriptor.name != nullptr) {
            hook_types.emplace_back(descriptor.name);
        }
    }

    if (hook_types.empty()) {
        return false;
    }

    process_command_candidates(cenv, hook_types, prefix, prefix_len, "hook type",
                               [](const std::string& value) { return value; });
    return ic_has_completions(cenv);
}

bool add_builtin_argument_completions(ic_completion_env_t* cenv,
                                      const std::vector<std::string>& tokens,
                                      bool ends_with_space) {
    if (tokens.empty() || cenv == nullptr) {
        return false;
    }
    if (ic_stop_completing(cenv) || completion_tracker::completion_limit_hit()) {
        return false;
    }

    ArgumentCompletionContext context;
    if (!build_argument_completion_context(tokens, ends_with_space, context)) {
        return false;
    }

    const std::string& command = tokens[0];
    size_t prefix_len = context.current_prefix.size();

    auto matches_command = [&](const char* name) {
        return completion_utils::equals_completion_token(command, name);
    };

    if (matches_command("builtin")) {
        if (context.argument_index != 1 || g_shell == nullptr ||
            g_shell->get_built_ins() == nullptr) {
            return false;
        }
        auto builtin_cmds = g_shell->get_built_ins()->get_builtin_commands();
        add_builtin_command_candidates(cenv, builtin_cmds, context.current_prefix, prefix_len);
        return ic_has_completions(cenv);
    }

    if (matches_command("alias") || matches_command("unalias")) {
        if (context.argument_index < 1 || g_shell == nullptr) {
            return false;
        }
        if (context.current_prefix.find('=') != std::string::npos) {
            return false;
        }
        const auto& alias_map = g_shell->get_aliases();
        auto alias_names = collect_map_keys(alias_map);
        auto alias_source_provider = make_map_source_provider(&alias_map);
        process_command_candidates(
            cenv, alias_names, context.current_prefix, prefix_len, "alias",
            [](const std::string& value) { return value; },
            std::function<bool(const std::string&)>{}, alias_source_provider);
        return ic_has_completions(cenv);
    }

    if (matches_command("abbr") || matches_command("abbreviate") || matches_command("unabbr") ||
        matches_command("unabbreviate")) {
        if (context.argument_index < 1 || g_shell == nullptr) {
            return false;
        }
        if (context.current_prefix.find('=') != std::string::npos) {
            return false;
        }
        const auto& abbr_map = g_shell->get_abbreviations();
        auto abbr_names = collect_map_keys(abbr_map);
        auto abbr_source_provider = make_map_source_provider(&abbr_map);
        process_command_candidates(
            cenv, abbr_names, context.current_prefix, prefix_len, "abbreviation",
            [](const std::string& value) { return value; },
            std::function<bool(const std::string&)>{}, abbr_source_provider);
        return ic_has_completions(cenv);
    }

    if (matches_command("type") || matches_command("which")) {
        if (context.argument_index < 1) {
            return false;
        }

        bool options_ended = false;
        if (context.argument_index > 1) {
            for (size_t index = 1; index < context.argument_index; ++index) {
                if (completion_utils::equals_completion_token(tokens[index], "--")) {
                    options_ended = true;
                    break;
                }
            }
        }

        if (!options_ended && !context.current_prefix.empty() && context.current_prefix[0] == '-') {
            return false;
        }

        auto sources = collect_command_completion_sources(true);

        add_builtin_command_candidates(cenv, sources.builtin_cmds, context.current_prefix,
                                       prefix_len);
        if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv)) {
            return ic_has_completions(cenv);
        }

        {
            const auto& control_structures = control_structure_keywords();
            process_command_candidates(cenv, control_structures, context.current_prefix, prefix_len,
                                       "control structure",
                                       [](const std::string& value) { return value; });
            if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv)) {
                return ic_has_completions(cenv);
            }
        }

        process_command_candidates(cenv, sources.function_names, context.current_prefix, prefix_len,
                                   "function", [](const std::string& value) { return value; });
        if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv)) {
            return ic_has_completions(cenv);
        }

        auto alias_source_provider = make_map_source_provider(sources.alias_map);
        process_command_candidates(
            cenv, sources.alias_names, context.current_prefix, prefix_len, "alias",
            [](const std::string& value) { return value; },
            std::function<bool(const std::string&)>{}, alias_source_provider);
        if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv)) {
            return ic_has_completions(cenv);
        }

        auto abbreviation_source_provider = make_map_source_provider(sources.abbreviation_map);
        process_command_candidates(
            cenv, sources.abbreviation_names, context.current_prefix, prefix_len, "abbreviation",
            [](const std::string& value) { return value; },
            std::function<bool(const std::string&)>{}, abbreviation_source_provider);
        if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv)) {
            return ic_has_completions(cenv);
        }

        size_t summary_fetch_budget = prefix_len == 0 ? 2 : 5;
        auto system_summary_provider = [&](const std::string& cmd) -> std::string {
            std::string summary = get_command_summary(cmd, false);
            if (!summary.empty())
                return summary;
            if (!config::completion_learning_enabled || summary_fetch_budget == 0)
                return {};
            --summary_fetch_budget;
            return get_command_summary(cmd, true);
        };

        process_command_candidates(
            cenv, sources.executables_in_path, context.current_prefix, prefix_len,
            "system installed command", [](const std::string& value) { return value; },
            std::function<bool(const std::string&)>{}, system_summary_provider);

        return ic_has_completions(cenv);
    }

    if (matches_command("hook")) {
        if (tokens.size() < 2) {
            return false;
        }
        const std::string& subcommand = tokens[1];
        bool is_add = completion_utils::equals_completion_token(subcommand, "add");
        bool is_remove = completion_utils::equals_completion_token(subcommand, "remove");
        bool is_list = completion_utils::equals_completion_token(subcommand, "list");
        bool is_clear = completion_utils::equals_completion_token(subcommand, "clear");

        if ((is_add || is_remove || is_list || is_clear) && context.argument_index == 2) {
            return add_hook_type_completions(cenv, context.current_prefix, prefix_len);
        }

        if ((is_add || is_remove) && context.argument_index == 3 && g_shell != nullptr) {
            std::vector<std::string> candidates;
            if (tokens.size() >= 3) {
                auto hook_type = parse_hook_type(tokens[2]);
                if (hook_type.has_value()) {
                    candidates = g_shell->get_hooks(*hook_type);
                }
            }

            if (candidates.empty() && g_shell->get_shell_script_interpreter() != nullptr) {
                candidates = g_shell->get_shell_script_interpreter()->get_function_names();
            }

            if (!candidates.empty()) {
                process_command_candidates(cenv, candidates, context.current_prefix, prefix_len,
                                           "function",
                                           [](const std::string& value) { return value; });
            }
            return ic_has_completions(cenv);
        }
    }

    if (matches_command("cjshopt")) {
        if (tokens.size() >= 2 &&
            completion_utils::equals_completion_token(tokens[1], "style_def") &&
            context.argument_index == 2 &&
            (context.current_prefix.empty() || context.current_prefix[0] != '-')) {
            const auto& styles = token_constants::default_styles();
            std::vector<std::string> style_tokens;
            style_tokens.reserve(styles.size() + 1);
            style_tokens.push_back("preview");
            for (const auto& entry : styles) {
                style_tokens.push_back(entry.first);
            }
            process_command_candidates(cenv, style_tokens, context.current_prefix, prefix_len,
                                       "style token",
                                       [](const std::string& value) { return value; });
            return ic_has_completions(cenv);
        }
    }

    return false;
}

}  // namespace

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix) {
    std::string prefix_str;
    size_t prefix_len = 0;
    if (!prepare_prefix_state(cenv, prefix, prefix_str, prefix_len))
        return;

    auto sources = collect_command_completion_sources(true);

    add_builtin_command_candidates(cenv, sources.builtin_cmds, prefix_str, prefix_len);
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    const auto& control_structures = control_structure_keywords();
    process_command_candidates(
        cenv, control_structures, prefix_str, prefix_len, "control structure",
        [](const std::string& value) { return value; }, std::function<bool(const std::string&)>{},
        builtin_summary_for_command);
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    process_command_candidates(cenv, sources.function_names, prefix_str, prefix_len, "function",
                               [](const std::string& value) { return value; });
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    auto alias_source_provider = make_map_source_provider(sources.alias_map);
    process_command_candidates(
        cenv, sources.alias_names, prefix_str, prefix_len, "alias",
        [](const std::string& value) { return value; }, std::function<bool(const std::string&)>{},
        alias_source_provider);
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    auto abbreviation_source_provider = make_map_source_provider(sources.abbreviation_map);
    process_command_candidates(
        cenv, sources.abbreviation_names, prefix_str, prefix_len, "abbreviation",
        [](const std::string& value) { return value; }, std::function<bool(const std::string&)>{},
        abbreviation_source_provider);
    if (completion_tracker::completion_limit_hit() || ic_stop_completing(cenv))
        return;

    size_t summary_fetch_budget = prefix_len == 0 ? 2 : 5;
    auto system_summary_provider = [&](const std::string& cmd) -> std::string {
        std::string summary = get_command_summary(cmd, false);
        if (!summary.empty())
            return summary;
        if (!config::completion_learning_enabled || summary_fetch_budget == 0)
            return {};
        --summary_fetch_budget;
        return get_command_summary(cmd, true);
    };

    process_command_candidates(
        cenv, sources.executables_in_path, prefix_str, prefix_len, "system installed command",
        [](const std::string& value) { return value; }, {}, system_summary_provider);

    if (!ic_has_completions(cenv) && g_completion_spell_correction_enabled) {
        std::string normalized_prefix = completion_utils::normalize_for_comparison(prefix_str);
        if (completion_spell::should_consider_spell_correction(normalized_prefix)) {
            std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> spell_matches;

            completion_spell::collect_spell_correction_candidates(
                sources.builtin_cmds, [](const std::string& value) { return value; },
                [](const std::string& cmd) { return is_interactive_builtin(cmd); },
                normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                sources.function_names, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                sources.alias_names, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                sources.abbreviation_names, [](const std::string& value) { return value; },
                std::function<bool(const std::string&)>{}, normalized_prefix, spell_matches);

            completion_spell::collect_spell_correction_candidates(
                sources.executables_in_path, [](const std::string& value) { return value; },
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
    std::string prefix_str;
    size_t prefix_len = 0;
    if (!prepare_prefix_state(cenv, prefix, prefix_str, prefix_len))
        return;

    std::ifstream history_file(cjsh_filesystem::g_cjsh_history_path());
    if (!history_file.is_open()) {
        return;
    }

    struct HistoryMatch {
        std::string command;
        bool has_exit_code;
        int exit_code;
    };

    std::vector<HistoryMatch> matches;
    matches.reserve(50);

    std::string line;
    line.reserve(256);
    std::string decoded_line;
    decoded_line.reserve(256);

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

        if (!decode_history_command_line(line, decoded_line)) {
            decoded_line = line;
        }
        const std::string& entry_text = decoded_line;

        if (looks_like_file_path(entry_text)) {
            last_exit_code = 0;
            has_last_exit_code = false;
            continue;
        }

        bool should_match = false;
        if (prefix_len == 0) {
            should_match = (entry_text != prefix_str);
        } else if (completion_utils::matches_completion_prefix(entry_text, prefix_str) &&
                   entry_text != prefix_str) {
            should_match = true;
        }

        if (should_match) {
            matches.push_back(HistoryMatch{entry_text, has_last_exit_code, last_exit_code});
        }

        last_exit_code = 0;
        has_last_exit_code = false;

        line.clear();
    }

    const size_t max_suggestions = 15;
    size_t count = 0;

    for (const auto& match : matches) {
        if (completion_tracker::completion_limit_hit())
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

    auto complete_special_prefix = [&](const std::string& dir_to_complete, bool treat_as_directory,
                                       const char* debug_label) {
        namespace fs = std::filesystem;
        fs::path dir_path;
        std::string match_prefix;
        determine_directory_target(dir_to_complete, treat_as_directory, dir_path, match_prefix);

        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, false, false,
                                               debug_label))
                    return false;
            }
        } catch (const std::exception&) {
            // Best-effort completion: ignore filesystem errors.
        }

        return true;
    };

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
        std::string dir_to_complete = cjsh_filesystem::g_user_home_path().string();

        if (unquoted_special.length() > 1) {
            dir_to_complete += "/" + path_after_tilde;
        }

        bool treat_as_directory = !unquoted_special.empty() && unquoted_special.back() == '/';
        if (!complete_special_prefix(dir_to_complete, treat_as_directory, "tilde"))
            return;
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

        bool treat_as_directory = !unquoted_special.empty() && unquoted_special.back() == '/';
        if (!complete_special_prefix(dir_to_complete, treat_as_directory, "dash"))
            return;
        return;
    }

    if (!prefix_before.empty()) {
        std::string command_part = prefix_before;

        while (!command_part.empty() &&
               (command_part.back() == ' ' || command_part.back() == '\t')) {
            command_part.pop_back();
        }
    }

    const bool has_command_prefix = !prefix_before.empty();
    std::string raw_path_input = has_command_prefix ? special_part : prefix_str;
    std::string path_to_check = completion_utils::unquote_path(raw_path_input);
    bool restrict_to_executables =
        !has_command_prefix && completion_utils::starts_with_case_sensitive(path_to_check, "./");

    std::function<bool(const std::filesystem::directory_entry&)> entry_filter;
    if (restrict_to_executables) {
        entry_filter = [](const std::filesystem::directory_entry& entry) {
            return is_executable_or_script_entry(entry);
        };
    }

    if (!ic_stop_completing(cenv) && !path_to_check.empty() && path_to_check.back() == '/') {
        namespace fs = std::filesystem;
        fs::path dir_path(path_to_check);
        try {
            if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
                bool had_completions_before = ic_has_completions(cenv);
                if (!iterate_directory_entries(cenv, dir_path, "", directories_only, false,
                                               "all files", entry_filter, restrict_to_executables))
                    return;

                if (directories_only && !ic_has_completions(cenv) && !had_completions_before) {
                    if (!iterate_directory_entries(cenv, dir_path, "", false, false,
                                                   "all files (fallback)", entry_filter,
                                                   restrict_to_executables))
                        return;
                }
            }
        } catch (const std::exception& e) {
            // Best-effort completion: ignore filesystem errors.
        }
        return;
    }

    std::string path_to_complete = completion_utils::unquote_path(raw_path_input);

    namespace fs = std::filesystem;
    fs::path dir_path;
    std::string match_prefix;
    bool treat_as_directory = path_to_complete.empty() || path_to_complete.back() == '/';
    determine_directory_target(path_to_complete, treat_as_directory, dir_path, match_prefix);

    try {
        if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
            if (directories_only) {
                bool had_completions_before = ic_has_completions(cenv);
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, true, true,
                                               "directory-only", entry_filter,
                                               restrict_to_executables))
                    return;

                if (!ic_has_completions(cenv) && !had_completions_before && match_prefix.empty()) {
                    if (!iterate_directory_entries(cenv, dir_path, "", false, true,
                                                   "all files (fallback)", entry_filter,
                                                   restrict_to_executables))
                        return;
                }
            } else {
                if (!iterate_directory_entries(cenv, dir_path, match_prefix, false, true,
                                               "general filename", entry_filter,
                                               restrict_to_executables))
                    return;
            }
        }
    } catch (const std::exception&) {
        // Best-effort completion: ignore filesystem errors.
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
            add_variable_completions(cenv, current_line_prefix);
            if (ic_stop_completing(cenv)) {
                completion_tracker::completion_session_end();
                return;
            }
            cjsh_filename_completer(cenv, current_line_prefix);
            if (ic_has_completions(cenv) && ic_stop_completing(cenv)) {
                completion_tracker::completion_session_end();
                return;
            }

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

            break;

        case CONTEXT_PATH:
            add_variable_completions(cenv, current_line_prefix);
            if (ic_stop_completing(cenv)) {
                completion_tracker::completion_session_end();
                return;
            }
            cjsh_history_completer(cenv, current_line_prefix);
            cjsh_filename_completer(cenv, current_line_prefix);
            break;

        case CONTEXT_ARGUMENT: {
            std::string prefix_str(current_line_prefix);
            std::vector<std::string> tokens = completion_utils::tokenize_command_line(prefix_str);

            add_variable_completions(cenv, prefix_str);
            if (ic_stop_completing(cenv)) {
                completion_tracker::completion_session_end();
                return;
            }

            bool ends_with_space =
                !prefix_str.empty() && std::isspace(static_cast<unsigned char>(prefix_str.back()));

            add_job_control_argument_completions(cenv, tokens, ends_with_space);

            add_builtin_argument_completions(cenv, tokens, ends_with_space);

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
                if (config::history_enabled) {
                    cjsh_history_completer(cenv, current_line_prefix);
                }
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
        print_error(
            {ErrorType::RUNTIME_ERROR,
             ErrorSeverity::WARNING,
             "completions",
             "failed to enforce history limit; history file may exceed the configured size.",
             {"Check disk permissions or trim the history file manually."}});
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

bool set_completion_max_results(long max_results, std::string* error_message) {
    return completion_tracker::set_completion_max_results(max_results, error_message);
}

long get_completion_max_results() {
    return completion_tracker::get_completion_max_results();
}

long get_completion_default_max_results() {
    return completion_tracker::get_completion_default_max_results();
}

long get_completion_min_allowed_results() {
    return completion_tracker::get_completion_min_allowed_results();
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
