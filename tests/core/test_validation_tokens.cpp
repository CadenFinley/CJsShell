/*
  test_validation_tokens.cpp

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

#include <array>
#include <cstdio>
#include <locale>
#include <memory>
#include <string>
#include <vector>

#include "interpreter.h"
#include "shell.h"
#include "shell_env.h"
#include "validation_common.h"

std::unique_ptr<Shell> g_shell;

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "[FAIL] %s\n", message);
    }
    return condition;
}

struct CommaWhitespace : std::ctype<char> {
    CommaWhitespace() : std::ctype<char>(classification()) {
    }

    static const mask* classification() {
        static const auto masks = [] {
            std::array<mask, table_size> result{};
            for (size_t i = 0; i < result.size(); ++i) {
                result[i] = classic_table()[i];
            }
            result[','] |= space;
            result[' '] &= ~space;
            return result;
        }();
        return masks.data();
    }
};

bool test_whitespace_and_locale() {
    using shell_validation::internal::tokenize_and_get_first;
    using shell_validation::internal::tokenize_whitespace;
    const std::locale previous = std::locale::global(std::locale::classic());
    bool ok = true;
    const std::vector<std::pair<std::string, std::vector<std::string>>> cases = {
        {"", {}},
        {" \t\r\n\v\f", {}},
        {"  for\ti\nin\vone\ftwo\r\n", {"for", "i", "in", "one", "two"}},
        {"if; then", {"if;", "then"}},
        {"\"one two\" 'three four'", {"\"one", "two\"", "'three", "four'"}},
        {std::string("a\0b c", 5), {std::string("a\0b", 3), "c"}},
        {"caf\xc3\xa9 next", {"caf\xc3\xa9", "next"}},
    };
    for (const auto& [input, expected] : cases) {
        const auto [tokens, first] = tokenize_and_get_first(input);
        ok = expect(tokens == expected, "validation preserves whitespace token boundaries") && ok;
        ok = expect(first == (expected.empty() ? "" : expected.front()),
                    "validation preserves the first token") &&
             ok;
    }
    std::locale::global(std::locale(std::locale::classic(), new CommaWhitespace));
    ok = expect(tokenize_whitespace(",one two,,three,") ==
                    std::vector<std::string>({"one two", "three"}),
                "validation honors the current C++ locale's whitespace") &&
         ok;
    std::locale::global(previous);
    return ok;
}

bool test_variable_diagnostics() {
    auto* interpreter = g_shell->get_shell_script_interpreter();
    const std::vector<std::string> lines = {
        "export __audit_export='two words'; read -r -p 'prompt words' __audit_read; "
        "declare __audit_declared=value",
        ": \"$__audit_export\" \"$__audit_read\" \"$__audit_declared\" \"$__audit_missing\"",
    };
    auto errors = interpreter->validate_variable_usage(lines);
    bool ok = expect(errors.size() == 1 && errors.front().error_code == "VAR002" &&
                         errors.front().position.line_number == 2 &&
                         errors.front().message.find("__audit_missing") != std::string::npos,
                     "declarations and read operands retain undefined-variable diagnostics");
    interpreter->get_variable_manager().set_environment_variable("__audit_missing", "defined");
    ok = expect(interpreter->validate_variable_usage(lines).empty(),
                "a subsequent validation sees newly defined variables") &&
         ok;
    const auto malformed = interpreter->validate_variable_usage({": \"${__audit_unclosed\""});
    bool found = false;
    for (const auto& error : malformed) {
        found =
            found || (error.error_code == "SYN008" && error.severity == ErrorSeverity::CRITICAL);
    }
    ok = expect(found, "unclosed parameter expansion remains a critical syntax error") && ok;
    return ok;
}

}  // namespace

int main() {
    cjsh_env::reset_shell_state();
    cjsh_env::set_startup_active(false);
    config::interactive_mode = false;
    config::force_interactive = false;
    g_shell = std::make_unique<Shell>();
    g_shell->set_interactive_mode(false);
    const bool tokens_ok = test_whitespace_and_locale();
    const bool diagnostics_ok = test_variable_diagnostics();
    g_shell.reset();
    if (tokens_ok && diagnostics_ok) {
        std::puts("All 2 validation token tests passed");
        return 0;
    }
    std::fprintf(stderr, "%d/2 validation token tests failed\n", !tokens_ok + !diagnostics_ok);
    return 1;
}
