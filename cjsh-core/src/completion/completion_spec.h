/*
  completion_spec.h

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

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace completion_specs {

inline constexpr unsigned int kCompletionSpecFormatVersion = 2;

enum class EntryKind : std::uint8_t {
    Option,
    Subcommand,
    Positional
};

enum class ValueRequirement : std::uint8_t {
    None,
    Required,
    Optional
};

enum class ValueType : std::uint8_t {
    None,
    Text,
    File,
    Directory,
    Enum,
    Command,
    Branch,
    Process,
    Custom
};

enum class ValueSeparator : std::uint8_t {
    Space,
    Equals,
    Either
};

struct CompletionValueSpec {
    ValueRequirement requirement{ValueRequirement::None};
    ValueType type{ValueType::None};
    ValueSeparator separator{ValueSeparator::Space};
    std::string name;
    std::vector<std::string> choices;
    std::string dynamic_provider;
};

struct CompletionEntry {
    // Keep these first three members stable so existing aggregate initializers remain valid.
    std::string text;
    std::string description;
    EntryKind kind{EntryKind::Option};

    std::vector<std::string> aliases;
    CompletionValueSpec value;
    std::vector<std::string> conflicts;
    std::vector<std::string> dependencies;
    bool repeatable{false};
    bool deprecated{false};

    // Positional indices are one-based. Zero means "infer from declaration order".
    std::size_t positional_index{0};
    bool variadic{false};

    // Subcommands can own a complete nested command specification.
    std::vector<CompletionEntry> children;
};

struct CommandDoc {
    std::vector<CompletionEntry> entries;
    std::string summary;
    std::string executable_path;
    bool summary_present{false};
};

struct DynamicCompletionRequest {
    std::string command;
    std::vector<std::string> command_path;
    std::vector<std::string> arguments;
    std::size_t argument_index{0};
    std::string current_value;
    std::string working_directory;
    CompletionValueSpec value;
};

struct DynamicCompletionCandidate {
    std::string value;
    std::string description;
};

using DynamicCompletionProvider =
    std::function<std::vector<DynamicCompletionCandidate>(const DynamicCompletionRequest&)>;

bool register_command_doc(const std::string& command, CommandDoc doc);
bool unregister_command_doc(const std::string& command);
std::optional<CommandDoc> lookup_registered_command_doc(const std::string& command);

bool register_dynamic_completion_provider(const std::string& name,
                                          DynamicCompletionProvider provider);
bool unregister_dynamic_completion_provider(const std::string& name);
std::vector<DynamicCompletionCandidate> request_dynamic_completions(
    const std::string& name, const DynamicCompletionRequest& request);

bool entry_matches_token(const CompletionEntry& entry, const std::string& token);
std::vector<std::string> entry_names(const CompletionEntry& entry);
std::string default_provider_for_value_type(ValueType type);

std::string serialize_command_doc(const std::string& command, const CommandDoc& doc);
std::optional<CommandDoc> parse_command_doc(const std::string& command,
                                            const std::string& contents);

const char* value_requirement_name(ValueRequirement requirement);
const char* value_type_name(ValueType type);
const char* value_separator_name(ValueSeparator separator);

}  // namespace completion_specs
