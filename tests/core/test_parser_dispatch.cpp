/*
  test_parser_dispatch.cpp

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

#include "builtin_help.h"
#include "interpreter_utils.h"
#include "parser.h"
#include "shell.h"
#include "shell_env.h"

std::unique_ptr<Shell> g_shell;

namespace {
size_t checks = 0;
size_t failures = 0;

void expect(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "[FAIL] %s\n", message);
    }
}

void test_logical_commands(Parser& parser) {
    // Without an active logical operator, preserve the original input verbatim.
    for (const std::string input :
         {"", " \t\n", ": word", " : one; : two ", ": a | b", ": 'a && b'", ": \"a || b\"",
          "if true; then :; fi", ": \"unterminated"}) {
        const auto commands = parser.parse_logical_commands(input);
        expect(input.empty()
                   ? commands.empty()
                   : commands.size() == 1 && commands[0].command == input && commands[0].op.empty(),
               "logical parsing preserves unsplit text and whitespace");
    }
    const auto commands = parser.parse_logical_commands(": one && : two || : three");
    expect(commands.size() == 3 && commands[0].command == ": one " && commands[0].op == "&&" &&
               commands[1].command == " : two " && commands[1].op == "||" &&
               commands[2].command == " : three" && commands[2].op.empty(),
           "logical parsing retains operator ordering and operand whitespace");
    const auto nested = parser.parse_logical_commands("{ : one && : two; } || : three");
    expect(nested.size() == 2 && nested[0].command == "{ : one && : two; } " &&
               nested[0].op == "||" && nested[1].command == " : three",
           "logical operators inside command groups do not split the outer command");
}

void test_semicolon_commands(Parser& parser) {
    struct Case {
        std::string input;
        bool newlines;
        std::vector<std::string> expected;
    };
    const std::vector<Case> cases = {
        {"", false, {}},
        {" \t\r\n", false, {}},
        {" \t: word\r\n", false, {": word"}},
        {": \"one two\"", false, {": \"one two\""}},
        {": one\n: two", false, {": one\n: two"}},
        {": one\n: two", true, {": one", ": two"}},
        {": \"one\ntwo\"", true, {": \"one\ntwo\""}},
        {"; : one;; : two;", false, {": one", ": two"}},
        {": \"one;two\"; : three", false, {": \"one;two\"", ": three"}},
        {": one\\;two; : three", false, {": one\\;two", ": three"}},
        {"{ : one; : two; }; : three", false, {"{ : one; : two; }", ": three"}},
        {"if true; then :; fi; : next", false, {"if true; then :; fi", ": next"}},
        {std::string(8192, 'x'), false, {std::string(8192, 'x')}},
        {std::string(": a\0b", 6), false, {std::string(": a\0b", 6)}},
    };
    for (const auto& test : cases) {
        expect(parser.parse_semicolon_commands(test.input, test.newlines) == test.expected,
               "semicolon parsing preserves quoting, groups, escapes, and newline policy");
    }
}

void test_comments() {
    using shell_script_interpreter::detail::strip_inline_comment;
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"", ""},
        {" : word \t", " : word \t"},
        {": 'one two'", ": 'one two'"},
        {": value # comment", ": value "},
        {": '#' \"#\" # comment", ": '#' \"#\" "},
        {": $# ${#value} ${value#prefix} # comment", ": $# ${#value} ${value#prefix} "},
        {std::string(8192, 'x'), std::string(8192, 'x')},
        {std::string("a\0b", 3), std::string("a\0b", 3)},
    };
    for (const auto& [input, expected] : cases) {
        expect(strip_inline_comment(input) == expected,
               "comment stripping preserves literals, parameters, and arbitrary bytes");
    }
}

void test_ampersand_commands() {
    using shell_script_interpreter::detail::split_ampersand;
    const std::vector<std::pair<std::string, std::vector<std::string>>> cases = {
        {"", {}},
        {" \t\r\n", {}},
        {" \t: word\r\n", {": word"}},
        {": 'one two'", {": 'one two'"}},
        {": $((1 + 2))", {": $((1 + 2))"}},
        {std::string(8192, 'x'), {std::string(8192, 'x')}},
        {std::string("a\0b", 3), {std::string("a\0b", 3)}},
        {": one & : two", {": one &", ": two"}},
        {": 'one&two'", {": 'one&two'"}},
        {": one\\&two", {": one\\&two"}},
        {": one && : two", {": one && : two"}},
        {": >&2", {": >&2"}},
        {": &>out", {": &>out"}},
        {": $((1 & 2))", {": $((1 & 2))"}},
        {"[[ one & two ]]", {"[[ one & two ]]"}},
    };
    for (const auto& [input, expected] : cases) {
        expect(split_ampersand(input) == expected,
               "ampersand splitting preserves literals, arithmetic, redirections and background "
               "lists");
    }
}

void test_help() {
    std::ostringstream output;
    auto* previous = std::cout.rdbuf(output.rdbuf());
    const std::vector<std::string> dynamic_help = {"dynamic help", "second line"};
    expect(!builtin_handle_help({":"}, {"literal help"}) && output.str().empty(),
           "ordinary builtin calls do not print inline help");
    expect(!builtin_handle_help({":", "operand", "--help"}, {"literal help"}),
           "first-argument help ignores later operands");
    expect(builtin_handle_help({":", "--help"}, {"literal help", "second line"}) &&
               output.str() == "literal help\nsecond line\n",
           "inline help prints all lines");
    output.str("");
    expect(builtin_handle_help({":", "operand", "--help"}, dynamic_help,
                               BuiltinHelpScanMode::AnyArgument) &&
               output.str() == "dynamic help\nsecond line\n",
           "owning help supports scanning all arguments");
    output.str("");
    expect(builtin_handle_help({":", "operand", "--help"}, {std::string("temporary ") + "help"},
                               BuiltinHelpScanMode::AnyArgument) &&
               output.str() == "temporary help\n",
           "inline help can borrow a temporary string for the duration of the call");
    output.str("");
    cjsh_env::set_startup_active(true);
    expect(builtin_handle_help_with_startup_guard({":", "--help"}, {"literal help"}) &&
               builtin_handle_help_with_startup_guard({":", "--help"}, dynamic_help) &&
               output.str().empty(),
           "both help representations suppress output during startup");
    cjsh_env::set_startup_active(false);
    expect(builtin_handle_help_with_startup_guard({":", "--help"}, {"literal help"}) &&
               output.str() == "literal help\n",
           "guarded inline help prints after startup");
    std::cout.rdbuf(previous);
}

void test_execution() {
    expect(g_shell->execute("dispatch_value=initial\n"
                            "false && dispatch_value=wrong\n"
                            "true || dispatch_value=wrong\n"
                            "true && dispatch_value='one;two'\n"
                            "dispatch_value=\"${dispatch_value#one;}\" # comment\n") == 0 &&
               cjsh_env::get_shell_variable_value("dispatch_value") == "two",
           "execution retains short circuiting, assignments, quotes, and parameter expansion");
    expect(g_shell->execute("dispatch_fn() { dispatch_value=$1; }\n"
                            "dispatch_fn first\ndispatch_fn second\n") == 0 &&
               cjsh_env::get_shell_variable_value("dispatch_value") == "second",
           "repeated function calls see current positional parameters");
    expect(g_shell->execute(": ignored --help") == 0 &&
               g_shell->execute("true ignored --help") == 0 &&
               g_shell->execute("false ignored --help") == 1,
           "boolean and null builtins retain status with non-help operands");
}
}  // namespace

int main() {
    cjsh_env::reset_shell_state();
    cjsh_env::set_startup_active(false);
    config::interactive_mode = false;
    config::force_interactive = false;
    g_shell = std::make_unique<Shell>();
    g_shell->set_interactive_mode(false);
    test_logical_commands(*g_shell->get_parser());
    test_semicolon_commands(*g_shell->get_parser());
    test_comments();
    test_ampersand_commands();
    test_help();
    test_execution();
    g_shell.reset();
    if (failures != 0) {
        std::fprintf(stderr, "%zu/%zu parser dispatch tests failed\n", failures, checks);
        return 1;
    }
    std::printf("All %zu parser dispatch tests passed\n", checks);
    return 0;
}
