/*
  test_function_syntax.cpp

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

#include "interpreter.h"
#include "shell.h"
#include "shell_env.h"
#include "token_classifier.h"

std::unique_ptr<Shell> g_shell;

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        (void)std::fprintf(stderr, "[FAIL] %s\n", message);
        return false;
    }
    return true;
}

bool test_function_keyword_with_parentheses() {
    const std::string definition = "function name() {}";
    ShellScriptInterpreter* interpreter = g_shell->get_shell_script_interpreter();
    if (!expect(interpreter != nullptr, "shell interpreter should be available")) {
        return false;
    }

    const auto syntax_errors = interpreter->validate_function_syntax({definition});
    bool ok = expect(syntax_errors.empty(),
                     "function name() should not include parentheses in validation");

    size_t name_start = 0;
    size_t name_end = 0;
    ok = expect(token_classifier::is_function_definition(definition, name_start, name_end),
                "function name() should be classified as a function definition") &&
         ok;
    ok = expect(definition.substr(name_start, name_end - name_start) == "name",
                "function classifier should return only the function name") &&
         ok;

    ok =
        expect(g_shell->execute(definition) == 0, "function name() should register successfully") &&
        ok;
    ok = expect(interpreter->has_function("name"),
                "registered function should use the name without parentheses") &&
         ok;
    ok = expect(!interpreter->has_function("name()"),
                "parentheses should not be part of the registered function name") &&
         ok;
    return ok;
}

bool test_invalid_keyword_function_name_still_fails_validation() {
    ShellScriptInterpreter* interpreter = g_shell->get_shell_script_interpreter();
    if (!expect(interpreter != nullptr, "shell interpreter should be available")) {
        return false;
    }

    const auto syntax_errors = interpreter->validate_function_syntax({"function bad-name() {}"});
    bool ok = expect(syntax_errors.size() == 1 && syntax_errors.front().error_code == "FUNC002",
                     "invalid characters in function names should still be rejected");

    const auto missing_name_errors = interpreter->validate_function_syntax({"function"});
    ok = expect(
             missing_name_errors.size() == 1 && missing_name_errors.front().error_code == "FUNC001",
             "the function keyword without a name should still be rejected") &&
         ok;
    return ok;
}

}  // namespace

int main() {
    cjsh_env::reset_shell_state();
    cjsh_env::set_startup_active(false);
    g_shell = std::make_unique<Shell>();
    g_shell->set_interactive_mode(false);

    size_t failures = 0;
    if (!test_function_keyword_with_parentheses()) {
        ++failures;
    }
    if (!test_invalid_keyword_function_name_still_fails_validation()) {
        ++failures;
    }

    if (failures != 0) {
        (void)std::fprintf(stderr, "%zu/2 function syntax tests failed\n", failures);
        return 1;
    }

    (void)std::printf("All 2 function syntax tests passed\n");
    return 0;
}
