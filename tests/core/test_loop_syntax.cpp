/*
  test_loop_syntax.cpp

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
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "interpreter.h"
#include "loop_evaluator.h"
#include "shell.h"
#include "shell_env.h"

std::unique_ptr<Shell> g_shell;

namespace {

size_t checks = 0;
size_t failures = 0;

void expect(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    }
}

void test_validation_and_continuation() {
    auto* interpreter = g_shell->get_shell_script_interpreter();
    const std::vector<std::vector<std::string>> invalid = {
        {"for i n {1..1000}; do", "echo $i", "done"},
        {"for i n one; do :; done"},
        {"for bad-name in one", "do", ":", "done"},
        {"for \"i\" in one; do :; done"},
        {"for i \"in\" one; do :; done"},
        {"select i n one; do", ":", "done"},
        {"select 1i in one; do :; done"},
        {"select $name in one; do :; done"},
        {"for ((i=0; i<2; i++)) extra; do :; done"},
        {"for ((i=0; i<2)); do :; done"},
    };
    for (const auto& lines : invalid) {
        expect(!interpreter->validate_loop_syntax(lines).empty(),
               "validator rejects " + lines.front());
        expect(interpreter->has_syntax_errors(lines, false),
               "execution validation blocks " + lines.front());
        expect(!interpreter->needs_additional_input(lines),
               "completed invalid loop does not keep requesting input: " + lines.front());
    }

    const std::vector<std::vector<std::string>> valid = {
        {"for i; do :; done"},
        {"for i", "do", ":", "done"},
        {"for i # comment", "do", ":", "done"},
        {"for i", "in one two", "do", ":", "done"},
        {"for i in ; do :; done"},
        {"for i in", "do", ":", "done"},
        {"for i in do done then; do :; done"},
        {"for _i2 in \"one two\" \"\"; do :; done"},
        {"select i; do :; done"},
        {"select i # comment", "do", ":", "done"},
        {"select i", "in one two", "do", ":", "done"},
        {"select i in do done then; do :; done"},
        {"select i in $empty; do :; done"},
        {"for ((i=0; i<2; i++)); do :; done"},
        {"for ((i=0; i<2; i++)) # comment", "do", ":", "done"},
    };
    for (const auto& lines : valid) {
        expect(interpreter->validate_loop_syntax(lines).empty(),
               "validator accepts " + lines.front());
        expect(!interpreter->needs_additional_input(lines),
               "complete valid loop does not request input: " + lines.front());
    }

    for (const std::string header :
         {"for i", "for i in one; do", "select i", "select i in one; do"}) {
        expect(interpreter->needs_additional_input({header}),
               "unfinished loop still requests input: " + header);
    }
}

void test_runtime_guards_without_validation() {
    // Exercise the evaluators directly: nested/prevalidated execution must also
    // diagnose malformed headers without running either body or trailing commands.
    const std::vector<std::string> headers = {
        "for i n one",
        "for 1i in one",
        "for \"i\" in one",
        "for i \"in\" one",
        "for",
        "for ((i=0; i<2))",
        "for ((i=0; i<2; i++)) extra",
        "select i n one",
        "select 1i in one",
        "select $name in one",
        "select",
        "select i in",
    };
    for (const auto& header : headers) {
        for (bool multiline : {false, true}) {
            const std::vector<std::string> lines =
                multiline ? std::vector<std::string>{header + "; do", ":", "done"}
                          : std::vector<std::string>{header + "; do :; done; echo TRAILING"};
            size_t index = 0;
            int calls = 0;
            auto body = [&](const std::vector<std::string>&) { return ++calls; };
            auto trailing = [&](const std::string&) { return ++calls; };
            std::ostringstream errors;
            auto* previous = std::cerr.rdbuf(errors.rdbuf());
            int rc = header.rfind("for", 0) == 0
                         ? loop_evaluator::handle_for_block(lines, index, body, nullptr, trailing,
                                                            g_shell->get_parser())
                         : loop_evaluator::handle_select_block(lines, index, body, trailing,
                                                               g_shell->get_parser());
            std::cerr.rdbuf(previous);

            expect(rc == 2, "runtime syntax status for " + header);
            expect(errors.str().find("syntax error") != std::string::npos,
                   "runtime diagnostic for " + header);
            expect(calls == 0, "invalid loop has no body/trailing effects: " + header);
            if (multiline) {
                expect(index == 2, "invalid multiline loop consumes its body: " + header);
            }
        }
    }
}

}  // namespace

int main() {
    cjsh_env::reset_shell_state();
    cjsh_env::set_startup_active(false);
    config::interactive_mode = false;
    config::force_interactive = false;
    g_shell = std::make_unique<Shell>();
    g_shell->set_interactive_mode(false);

    test_validation_and_continuation();
    test_runtime_guards_without_validation();
    g_shell.reset();
    std::printf("Loop syntax: %zu checks, %zu failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
