/*
  test_agent_mode.cpp

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

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "agent_mode.h"
#include "shell.h"

std::unique_ptr<Shell> g_shell;

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        (void)std::fprintf(stderr, "[FAIL] %s\n", message);
        return false;
    }
    return true;
}

bool test_parses_flyline_protocol_with_surrounding_prose() {
    const std::string output =
        "Here are two choices:\n```json\n[\n"
        "  {\"command\":\"find . -mtime +3\",\"description\":\"Old files\"},\n"
        "  {\"description\":\"Quoted and Unicode \\u2713\","
        "\"command\":\"printf \\\"ok\\\\n\\\"\",\"ignored\":{\"nested\":[1,true]}}\n"
        "]\n```\nChoose one.";

    std::vector<agent_mode::Suggestion> suggestions;
    std::string error;
    if (!expect(agent_mode::parse_suggestions(output, &suggestions, &error),
                "valid executor JSON should parse")) {
        return false;
    }
    return expect(error.empty(), "successful parse should clear its error") &&
           expect(suggestions.size() == 2, "both suggestions should be retained") &&
           expect(suggestions[0].command == "find . -mtime +3",
                  "the first command should round-trip") &&
           expect(suggestions[1].command == "printf \"ok\\n\"", "JSON escapes should be decoded") &&
           expect(suggestions[1].description == "Quoted and Unicode ✓",
                  "Unicode escapes should become UTF-8");
}

bool test_ignores_entries_without_commands() {
    const std::string output =
        "  [ {\"description\":\"missing\"}, {\"command\":\"   \"}, "
        "{\"command\":\"pwd\"} ]";
    std::vector<agent_mode::Suggestion> suggestions;
    return expect(agent_mode::parse_suggestions(output, &suggestions),
                  "an array with one usable command should parse") &&
           expect(suggestions.size() == 1, "empty commands should be discarded") &&
           expect(suggestions[0].command == "pwd", "usable command should remain");
}

bool test_rejects_malformed_or_unframed_output() {
    std::vector<agent_mode::Suggestion> suggestions;
    std::string error;
    if (!expect(!agent_mode::parse_suggestions("not json", &suggestions, &error),
                "plain prose should be rejected") ||
        !expect(!error.empty(), "rejected prose should explain the failure")) {
        return false;
    }
    if (!expect(!agent_mode::parse_suggestions("prefix [{\"command\":\"pwd\"}]", &suggestions),
                "an inline array should not be mistaken for the protocol")) {
        return false;
    }
    if (!expect(!agent_mode::parse_suggestions("[{\"command\": 42}]", &suggestions),
                "command must be a JSON string")) {
        return false;
    }
    return expect(!agent_mode::parse_suggestions("[]", &suggestions),
                  "an empty suggestion array should be rejected");
}

bool test_limits_suggestions_to_three() {
    const std::string output =
        R"([{"command":"one"},{"command":"two"},{"command":"three"},{"command":"four"}])";
    std::vector<agent_mode::Suggestion> suggestions;
    return expect(agent_mode::parse_suggestions(output, &suggestions),
                  "four valid executor suggestions should parse") &&
           expect(suggestions.size() == 3, "only three suggestions should reach the menu") &&
           expect(suggestions.back().command == "three", "suggestion ordering should be retained");
}

}  // namespace

int main() {
    const std::vector<std::pair<const char*, bool (*)()>> tests = {
        {"parse protocol", test_parses_flyline_protocol_with_surrounding_prose},
        {"ignore empty commands", test_ignores_entries_without_commands},
        {"reject malformed output", test_rejects_malformed_or_unframed_output},
        {"limit suggestions", test_limits_suggestions_to_three},
    };
    size_t failed = 0;
    for (const auto& [name, test] : tests) {
        if (!test()) {
            std::fprintf(stderr, "[FAIL] agent mode: %s\n", name);
            ++failed;
        }
    }
    if (failed != 0) {
        return 1;
    }
    std::printf("All agent-mode tests passed.\n");
    return 0;
}
