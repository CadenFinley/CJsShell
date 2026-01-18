#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace builtin_completions {

enum class EntryKind : std::uint8_t {
    Option,
    Subcommand
};

struct CompletionEntry {
    std::string text;
    std::string description;
    EntryKind kind;
};

struct CommandDoc {
    std::vector<CompletionEntry> entries;
    std::string summary;
    std::string executable_path;
    bool summary_present{false};
};

const CommandDoc* lookup_builtin_command_doc(const std::string& doc_target);
std::string get_builtin_summary(const std::string& command);

}  // namespace builtin_completions
