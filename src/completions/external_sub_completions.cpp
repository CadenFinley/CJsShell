#include "external_sub_completions.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cjsh_filesystem.h"
#include "completion_tracker.h"
#include "completion_utils.h"
#include "exec.h"

namespace {

enum class EntryKind : std::uint8_t {
    Option,
    Subcommand
};

struct CompletionEntry {
    std::string text;
    std::string description;
    EntryKind kind;
};

struct OptionState {
    std::vector<std::string> names;
    std::string description;
    bool active{false};
};

struct CommandState {
    std::string name;
    std::string description;
    bool active{false};
};

enum class Section : std::uint8_t {
    None,
    Options,
    Commands
};

std::mutex g_cache_mutex;
std::unordered_map<std::string, std::vector<CompletionEntry>> g_memory_cache;
std::unordered_set<std::string> g_failed_targets;

std::string trim_left(const std::string& value) {
    std::string::size_type pos = 0;
    while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos])) != 0) {
        ++pos;
    }
    return value.substr(pos);
}

std::string trim_right(const std::string& value) {
    if (value.empty())
        return value;
    std::string::size_type pos = value.size();
    while (pos > 0 && std::isspace(static_cast<unsigned char>(value[pos - 1])) != 0) {
        --pos;
    }
    return value.substr(0, pos);
}

std::string trim(const std::string& value) {
    return trim_left(trim_right(value));
}

std::string collapse_whitespace(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    bool in_space = false;
    for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!in_space) {
                result.push_back(' ');
                in_space = true;
            }
        } else {
            result.push_back(ch);
            in_space = false;
        }
    }
    return trim(result);
}

std::string shorten_description(const std::string& text) {
    constexpr std::size_t kMaxLen = 96;
    if (text.size() <= kMaxLen)
        return text;

    std::string truncated = text.substr(0, kMaxLen);
    std::size_t last_space = truncated.find_last_of(' ');
    if (last_space != std::string::npos && last_space > kMaxLen / 2) {
        truncated.erase(last_space);
    }
    truncated.append("...");
    return truncated;
}

std::string sanitize_description(const std::string& text) {
    std::string collapsed = collapse_whitespace(text);
    if (collapsed.empty())
        return collapsed;
    return shorten_description(collapsed);
}

bool has_lowercase(const std::string& value) {
    return std::any_of(value.begin(), value.end(),
                       [](unsigned char ch) { return std::islower(ch) != 0; });
}

std::string to_upper_copy(const std::string& value) {
    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return upper;
}

bool is_section_heading(const std::string& trimmed_line) {
    if (trimmed_line.empty())
        return false;

    bool has_alpha = false;
    for (unsigned char ch : trimmed_line) {
        if (std::islower(ch) != 0)
            return false;
        if (std::isalpha(ch) != 0)
            has_alpha = true;
    }
    return has_alpha;
}

Section section_from_heading(const std::string& heading) {
    std::string upper = to_upper_copy(heading);
    if (upper.find("OPTION") != std::string::npos)
        return Section::Options;
    if (upper.find("COMMAND") != std::string::npos || upper.find("SUBCOMMAND") != std::string::npos)
        return Section::Commands;
    return Section::None;
}

bool is_token_allowed_for_combination(const std::string& token) {
    if (token.empty())
        return false;
    if (token[0] == '-' || token[0] == '~')
        return false;
    if (token.find('/') != std::string::npos)
        return false;
    if (token.find('.') != std::string::npos)
        return false;

    bool has_alpha = false;
    for (unsigned char ch : token) {
        if (std::isalpha(ch) != 0)
            has_alpha = true;
        if ((std::isalnum(ch) == 0) && ch != '-' && ch != '_')
            return false;
    }
    return has_alpha;
}

std::pair<std::string, std::string> split_option_line(const std::string& line) {
    std::size_t double_space = line.find("  ");
    std::size_t tab_pos = line.find('\t');
    std::size_t split_pos = std::string::npos;

    if (double_space != std::string::npos)
        split_pos = double_space;
    if (tab_pos != std::string::npos && (split_pos == std::string::npos || tab_pos < split_pos))
        split_pos = tab_pos;
    if (split_pos == std::string::npos) {
        std::size_t first_space = line.find(' ');
        if (first_space != std::string::npos) {
            split_pos = line.find_first_not_of(' ', first_space);
        }
    }

    if (split_pos == std::string::npos) {
        return {trim_right(line), std::string{}};
    }

    std::string name_part = trim_right(line.substr(0, split_pos));
    std::string desc_part = trim(line.substr(split_pos));
    return {name_part, desc_part};
}

std::vector<std::string> parse_option_names(const std::string& spec) {
    std::vector<std::string> names;
    std::string current;
    for (char ch : spec) {
        if (ch == ',') {
            std::string cleaned = trim(current);
            if (!cleaned.empty())
                names.push_back(cleaned);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        std::string cleaned = trim(current);
        if (!cleaned.empty())
            names.push_back(cleaned);
    }

    std::vector<std::string> result;
    result.reserve(names.size());
    for (std::string name : names) {
        if (name.empty())
            continue;
        std::size_t space_pos = name.find_first_of(" \t");
        if (space_pos != std::string::npos)
            name = name.substr(0, space_pos);
        std::size_t bracket_pos = name.find_first_of("[{(");
        if (bracket_pos != std::string::npos)
            name = name.substr(0, bracket_pos);
        while (!name.empty() && (name.back() == ',' || name.back() == ';' || name.back() == '.')) {
            name.pop_back();
        }
        if (name.empty())
            continue;
        if (name[0] != '-')
            continue;
        result.push_back(name);
    }

    if (result.empty() && !spec.empty() && spec[0] == '-') {
        std::string fallback = spec;
        std::size_t space_pos = fallback.find_first_of(" \t");
        if (space_pos != std::string::npos)
            fallback = fallback.substr(0, space_pos);
        std::size_t bracket_pos = fallback.find_first_of("[{(");
        if (bracket_pos != std::string::npos)
            fallback = fallback.substr(0, bracket_pos);
        while (!fallback.empty() &&
               (fallback.back() == ',' || fallback.back() == ';' || fallback.back() == '.')) {
            fallback.pop_back();
        }
        if (!fallback.empty() && fallback[0] == '-')
            result.push_back(fallback);
    }

    return result;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    lines.reserve(512);
    std::string current;
    for (char ch : text) {
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    lines.push_back(current);
    return lines;
}

std::string sanitize_man_output(const std::string& raw) {
    std::string cleaned;
    cleaned.reserve(raw.size());
    for (char ch : raw) {
        if (ch == '\r')
            continue;
        if (ch == '\b') {
            if (!cleaned.empty())
                cleaned.pop_back();
            continue;
        }
        cleaned.push_back(ch);
    }
    return cleaned;
}

std::vector<std::string> build_prefixes(const std::string& doc_target) {
    std::vector<std::string> prefixes;
    prefixes.push_back(doc_target);

    std::string spaced = doc_target;
    std::replace(spaced.begin(), spaced.end(), '-', ' ');
    if (std::find(prefixes.begin(), prefixes.end(), spaced) == prefixes.end())
        prefixes.push_back(spaced);

    std::size_t dash_pos = doc_target.find('-');
    if (dash_pos != std::string::npos) {
        std::string base = doc_target.substr(0, dash_pos);
        if (std::find(prefixes.begin(), prefixes.end(), base) == prefixes.end())
            prefixes.push_back(base);
    }

    std::sort(prefixes.begin(), prefixes.end(),
              [](const std::string& a, const std::string& b) { return a.size() > b.size(); });

    return prefixes;
}

std::string strip_known_prefix(const std::string& line, const std::vector<std::string>& prefixes) {
    for (const auto& prefix : prefixes) {
        if (prefix.empty())
            continue;

        const std::string variants[] = {prefix + " ", prefix + "\t", prefix + "-",
                                        prefix + "::", prefix + ":"};
        for (const auto& variant : variants) {
            if (line.rfind(variant, 0) == 0) {
                return line.substr(variant.size());
            }
        }
    }
    return line;
}

std::optional<std::pair<std::string, std::string>> parse_command_line(
    const std::vector<std::string>& prefixes, const std::string& original_line) {
    std::string working = trim(original_line);
    if (working.empty())
        return std::nullopt;

    working = strip_known_prefix(working, prefixes);

    if (working.empty())
        return std::nullopt;

    if (working[0] == '-' || working[0] == '*') {
        std::size_t pos = working.find_first_not_of("-* ");
        if (pos != std::string::npos)
            working = working.substr(pos);
        else
            return std::nullopt;
    }

    if (working.empty() || !has_lowercase(working))
        return std::nullopt;

    std::size_t split_pos = working.find("  ");
    std::size_t tab_pos = working.find('\t');
    if (tab_pos != std::string::npos && (split_pos == std::string::npos || tab_pos < split_pos))
        split_pos = tab_pos;
    if (split_pos == std::string::npos) {
        std::size_t first_space = working.find(' ');
        if (first_space == std::string::npos)
            return std::nullopt;
        split_pos = first_space;
    }

    std::string name_part = trim(working.substr(0, split_pos));
    std::string description_part = trim(working.substr(split_pos));

    if (name_part.empty())
        return std::nullopt;

    std::size_t special_pos = name_part.find_first_of(" \t([{:");
    if (special_pos != std::string::npos)
        name_part = name_part.substr(0, special_pos);

    while (!name_part.empty() &&
           (name_part.back() == ':' || name_part.back() == ';' || name_part.back() == ',')) {
        name_part.pop_back();
    }

    if (name_part.empty())
        return std::nullopt;

    if (!std::isalpha(static_cast<unsigned char>(name_part[0])) && name_part[0] != '_')
        return std::nullopt;

    if (!has_lowercase(name_part))
        return std::nullopt;

    return std::make_pair(name_part, description_part);
}

void flush_option_state(OptionState& state, std::vector<CompletionEntry>& entries,
                        std::unordered_set<std::string>& seen) {
    if (!state.active || state.names.empty())
        return;

    std::string description = sanitize_description(state.description);
    if (description.empty())
        description = "option";

    for (const auto& name : state.names) {
        if (name.empty())
            continue;
        std::string key = "O|" + name;
        if (seen.insert(key).second) {
            entries.push_back({name, description, EntryKind::Option});
        }
    }

    state = OptionState{};
}

void flush_command_state(CommandState& state, std::vector<CompletionEntry>& entries,
                         std::unordered_set<std::string>& seen) {
    if (!state.active || state.name.empty())
        return;

    std::string description = sanitize_description(state.description);
    if (description.empty())
        description = "subcommand";

    std::string key = "S|" + state.name;
    if (seen.insert(key).second) {
        entries.push_back({state.name, description, EntryKind::Subcommand});
    }

    state = CommandState{};
}

std::vector<CompletionEntry> parse_man_text(const std::string& doc_target,
                                            const std::string& man_text) {
    std::vector<CompletionEntry> entries;
    entries.reserve(64);

    std::unordered_set<std::string> seen;
    OptionState option_state;
    CommandState command_state;
    Section section = Section::None;

    std::vector<std::string> prefixes = build_prefixes(doc_target);

    for (const std::string& raw_line : split_lines(man_text)) {
        std::string trimmed_line = trim(raw_line);

        if (trimmed_line.empty()) {
            flush_option_state(option_state, entries, seen);
            flush_command_state(command_state, entries, seen);
            continue;
        }

        if (is_section_heading(trimmed_line)) {
            flush_option_state(option_state, entries, seen);
            flush_command_state(command_state, entries, seen);
            section = section_from_heading(trimmed_line);
            continue;
        }

        std::string left_trimmed = trim_left(raw_line);

        if (section == Section::Options ||
            (!left_trimmed.empty() && left_trimmed[0] == '-' && section == Section::None)) {
            if (!left_trimmed.empty() && left_trimmed[0] == '-') {
                flush_option_state(option_state, entries, seen);

                auto split = split_option_line(left_trimmed);
                auto names = parse_option_names(split.first);
                if (!names.empty()) {
                    option_state.names = std::move(names);
                    option_state.description = split.second;
                    option_state.active = true;
                    continue;
                }
            } else if (option_state.active) {
                std::string extra = trim(left_trimmed);
                if (!extra.empty()) {
                    if (!option_state.description.empty())
                        option_state.description += ' ';
                    option_state.description += extra;
                }
                continue;
            }
        }

        if (section == Section::Options && option_state.active) {
            std::string extra = trim(left_trimmed);
            if (!extra.empty()) {
                if (!option_state.description.empty())
                    option_state.description += ' ';
                option_state.description += extra;
                continue;
            }
        }

        if (section == Section::Commands) {
            auto parsed = parse_command_line(prefixes, left_trimmed);
            if (parsed.has_value()) {
                flush_command_state(command_state, entries, seen);
                command_state.name = parsed->first;
                command_state.description = parsed->second;
                command_state.active = true;
                continue;
            } else if (command_state.active) {
                std::string extra = trim(left_trimmed);
                if (!extra.empty()) {
                    if (!command_state.description.empty())
                        command_state.description += ' ';
                    command_state.description += extra;
                }
                continue;
            }
        }

        if (option_state.active) {
            flush_option_state(option_state, entries, seen);
        }
        if (command_state.active) {
            flush_command_state(command_state, entries, seen);
        }
    }

    flush_option_state(option_state, entries, seen);
    flush_command_state(command_state, entries, seen);

    return entries;
}

std::string normalize_key(const std::string& value) {
    std::string key = value;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return key;
}

std::string sanitize_command_for_cache(const std::string& command) {
    std::string sanitized;
    sanitized.reserve(command.size());
    for (unsigned char ch : command) {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_') {
            sanitized.push_back(static_cast<char>(std::tolower(ch)));
        } else {
            sanitized.push_back('_');
        }
    }
    if (sanitized.empty())
        sanitized = "command";
    return sanitized;
}

std::vector<CompletionEntry> read_cache_entries(const std::filesystem::path& path) {
    std::vector<CompletionEntry> entries;
    auto result = cjsh_filesystem::read_file_content(path.string());
    if (result.is_error())
        return entries;

    std::istringstream stream(result.value());
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty())
            continue;
        std::size_t first_tab = line.find('\t');
        if (first_tab == std::string::npos || first_tab == 0)
            continue;
        char type = line[0];
        std::size_t second_tab = line.find('\t', first_tab + 1);
        if (second_tab == std::string::npos)
            continue;
        std::string value = line.substr(first_tab + 1, second_tab - first_tab - 1);
        std::string description = line.substr(second_tab + 1);
        if (value.empty())
            continue;
        EntryKind kind = (type == 'S') ? EntryKind::Subcommand : EntryKind::Option;
        entries.push_back({value, description, kind});
    }
    return entries;
}

void write_cache_entries(const std::filesystem::path& path,
                         const std::vector<CompletionEntry>& entries) {
    if (entries.empty())
        return;

    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
    (void)ignored;

    std::ostringstream stream;
    for (const auto& entry : entries) {
        std::string value = entry.text;
        std::replace(value.begin(), value.end(), '\t', ' ');
        std::string description = entry.description;
        std::replace(description.begin(), description.end(), '\t', ' ');
        stream << (entry.kind == EntryKind::Subcommand ? 'S' : 'O') << '\t' << value << '\t'
               << description << '\n';
    }

    (void)cjsh_filesystem::write_file_content(path.string(), stream.str());
}

std::string fetch_man_page_text(const std::string& target) {
    const std::vector<std::vector<std::string>> attempts = {{"man", "-P", "cat", target},
                                                            {"man", target}};

    for (const auto& args : attempts) {
        auto output = exec_utils::execute_command_vector_for_output(args);
        if (output.success && !output.output.empty()) {
            return sanitize_man_output(output.output);
        }
    }

    return {};
}

std::vector<CompletionEntry> load_entries_for_target(const std::string& doc_target) {
    if (doc_target.empty())
        return {};

    std::string key = normalize_key(doc_target);

    {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        auto memo_it = g_memory_cache.find(key);
        if (memo_it != g_memory_cache.end())
            return memo_it->second;
        if (g_failed_targets.find(key) != g_failed_targets.end())
            return {};
    }

    std::filesystem::path cache_path = cjsh_filesystem::g_cjsh_generated_completions_path /
                                       (sanitize_command_for_cache(doc_target) + ".txt");

    auto cached_entries = read_cache_entries(cache_path);
    if (!cached_entries.empty()) {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        g_memory_cache[key] = cached_entries;
        return cached_entries;
    }

    std::string man_text = fetch_man_page_text(doc_target);
    if (man_text.empty()) {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        g_failed_targets.insert(key);
        return {};
    }

    auto parsed_entries = parse_man_text(doc_target, man_text);
    if (parsed_entries.empty()) {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        g_failed_targets.insert(key);
        return {};
    }

    write_cache_entries(cache_path, parsed_entries);

    {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        g_memory_cache[key] = parsed_entries;
    }
    return parsed_entries;
}

std::vector<CompletionEntry> resolve_entries_for_tokens(const std::vector<std::string>& tokens,
                                                        std::size_t stable_count) {
    std::vector<CompletionEntry> merged;
    std::unordered_set<std::string> seen;

    if (tokens.empty())
        return merged;

    auto append_unique = [&](const std::vector<CompletionEntry>& entries) {
        for (const auto& entry : entries) {
            std::string key = (entry.kind == EntryKind::Subcommand ? "S|" : "O|") + entry.text;
            if (seen.insert(key).second)
                merged.push_back(entry);
        }
    };

    std::string current_doc = tokens[0];
    auto current_entries = load_entries_for_target(current_doc);
    append_unique(current_entries);

    std::size_t max_depth = std::min(stable_count, tokens.size());
    for (std::size_t index = 1; index < max_depth; ++index) {
        const std::string& token = tokens[index];
        if (!is_token_allowed_for_combination(token))
            break;

        bool matches_subcommand = std::any_of(
            current_entries.begin(), current_entries.end(), [&](const CompletionEntry& entry) {
                return entry.kind == EntryKind::Subcommand &&
                       completion_utils::equals_completion_token(entry.text, token);
            });

        if (!matches_subcommand)
            break;

        current_doc.push_back('-');
        current_doc += token;

        current_entries = load_entries_for_target(current_doc);
        if (current_entries.empty())
            continue;

        append_unique(current_entries);
    }

    return merged;
}

}  // namespace

void handle_external_sub_completions(ic_completion_env_t* cenv, const char* raw_path_input) {
    if (cenv == nullptr || raw_path_input == nullptr)
        return;
    if (ic_stop_completing(cenv))
        return;

    std::string line(raw_path_input);
    std::vector<std::string> tokens = completion_utils::tokenize_command_line(line);
    if (tokens.empty())
        return;

    bool ends_with_space =
        !line.empty() && std::isspace(static_cast<unsigned char>(line.back())) != 0;

    std::size_t stable_count = tokens.size();
    if (!ends_with_space && !tokens.empty()) {
        stable_count = tokens.size() - 1;
    }
    if (stable_count == 0)
        stable_count = 1;

    std::string current_prefix;
    if (!ends_with_space && !tokens.empty())
        current_prefix = tokens.back();

    auto completions = resolve_entries_for_tokens(tokens, stable_count);
    if (completions.empty())
        return;

    long delete_before = current_prefix.empty() ? 0 : static_cast<long>(current_prefix.size());

    std::size_t added = 0;
    for (const auto& entry : completions) {
        if (completion_tracker::completion_limit_hit_with_log("external command"))
            break;
        if (ic_stop_completing(cenv))
            break;

        if (!current_prefix.empty() &&
            !completion_utils::matches_completion_prefix(entry.text, current_prefix)) {
            continue;
        }

        std::string insert_text = entry.text;
        bool append_space = false;
        if (entry.kind == EntryKind::Subcommand) {
            append_space = true;
        } else if (entry.kind == EntryKind::Option) {
            if (insert_text.find('=') == std::string::npos &&
                insert_text.find('<') == std::string::npos &&
                insert_text.find('[') == std::string::npos) {
                append_space = true;
            }
        }

        if (append_space && !insert_text.empty() && insert_text.back() != ' ')
            insert_text.push_back(' ');

        std::string source = entry.description.empty()
                                 ? (entry.kind == EntryKind::Subcommand ? "subcommand" : "option")
                                 : entry.description;

        if (!completion_tracker::safe_add_completion_prim_with_source(
                cenv, insert_text.c_str(), nullptr, nullptr, source.c_str(), delete_before, 0)) {
            break;
        }
        ++added;
        if (added >= 120)
            break;
    }
}