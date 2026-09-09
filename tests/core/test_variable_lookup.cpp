/*
  test_variable_lookup.cpp

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

#include <fnmatch.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "flags.h"
#include "interpreter.h"
#include "parameter_expansion_evaluator.h"
#include "shell.h"
#include "shell_env.h"

std::unique_ptr<Shell> g_shell;

namespace {
bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "[FAIL] %s\n", message);
    }
    return condition;
}

bool test_variable_presence_and_scope() {
    auto& variables = g_shell->get_shell_script_interpreter()->get_variable_manager();
    bool ok = true;
    auto check = [&](const std::string& name, const std::string& value, bool present) {
        ok = expect(variables.get_variable_value(name) == value, (name + " value").c_str()) && ok;
        ok = expect(variables.variable_is_set(name) == present, (name + " presence").c_str()) && ok;
    };
    check("__lookup_missing", "", false);
    variables.set_environment_variable("__lookup_scalar", "");
    check("__lookup_scalar", "", true);
    check("__lookup_scalar[0]", "", true);
    check("__lookup_scalar[1]", "", false);
    variables.set_environment_variable("__lookup_scalar", "outer");
    check("__lookup_scalar[@]", "outer", true);
    setenv("__lookup_process", "", 1);
    check("__lookup_process", "", true);
    unsetenv("__lookup_process");

    ok = expect(variables.assign_global_array_literal("__lookup_array", {"[2]=", "[5]=five"}),
                "create sparse array") &&
         ok;
    check("__lookup_array", "", false);
    check("__lookup_array[2]", "", true);
    check("__lookup_array[1+4]", "five", true);
    check("__lookup_array[4]", "", false);
    check("__lookup_array[@]", " five", true);
    ok =
        expect(variables.assign_global_array_literal("__lookup_empty", {}), "create empty array") &&
        ok;
    check("__lookup_empty[@]", "", false);
    ok = expect(variables.assign_global_associative_literal("__lookup_assoc",
                                                            {"[label]=", "[other]=value"}),
                "create associative array") &&
         ok;
    check("__lookup_assoc[label]", "", true);
    check("__lookup_assoc[other]", "value", true);
    check("__lookup_assoc[missing]", "", false);
    variables.set_environment_variable("__lookup_key", "other");
    check("__lookup_assoc[$__lookup_key]", "value", true);
    ok = expect(variables.set_nameref("__lookup_ref", "__lookup_array", true), "create nameref") &&
         ok;
    check("__lookup_ref[5]", "five", true);

    variables.push_scope();
    variables.set_local_variable("__lookup_scalar", "local");
    check("__lookup_scalar", "local", true);
    variables.set_local_variable("__lookup_array", "");
    ok = expect(variables.assign_array_literal("__lookup_array", {"[1]=local"}),
                "create local array") &&
         ok;
    check("__lookup_ref[1]", "local", true);
    check("__lookup_ref[5]", "", false);
    variables.set_local_variable("__lookup_assoc", "");
    ok = expect(variables.assign_associative_literal("__lookup_assoc", {"[label]=local"}),
                "create local associative array") &&
         ok;
    check("__lookup_assoc[label]", "local", true);
    check("__lookup_assoc[other]", "", false);
    variables.pop_scope();
    check("__lookup_scalar", "outer", true);
    check("__lookup_ref[5]", "five", true);
    check("__lookup_assoc[other]", "value", true);

    flags::set_positional_parameters({"", "second"});
    check("1", "", true);
    check("2", "second", true);
    check("3", "", false);
    check("999999999999999999999999999999999", "", false);
    check("#", "2", true);
    check("@", " second", true);
    return ok;
}

bool test_parameter_expansion_work() {
    size_t presence_checks = 0;
    size_t pattern_calls = 0;
    std::string value(4096, 'a');
    bool present = true;
    ParameterExpansionEvaluator evaluator([&](const std::string&) { return value; },
                                          [&](const std::string&, const std::string& replacement) {
                                              value = replacement;
                                              present = true;
                                          },
                                          [&](const std::string&) {
                                              ++presence_checks;
                                              return present;
                                          },
                                          [&](const std::string& text, const std::string& pattern) {
                                              ++pattern_calls;
                                              return fnmatch(pattern.c_str(), text.c_str(), 0) == 0;
                                          });
    bool ok = true;
    for (const char* expression : {"v##*", "v%%*"}) {
        presence_checks = pattern_calls = 0;
        ok = expect(evaluator.expand(expression).empty(), "longest wildcard removes full value") &&
             ok;
        ok = expect(pattern_calls == 1, "longest wildcard stops after first match") && ok;
        ok = expect(presence_checks == 0, "pattern removal does not query presence") && ok;
    }
    for (const char* expression : {"v", "v#*", "v%*"}) {
        presence_checks = 0;
        ok = expect(evaluator.expand(expression) == value,
                    "plain/shortest expansion retains value") &&
             ok;
        ok = expect(presence_checks == 0, "value-only expansion does not query presence") && ok;
    }
    value = "abcabc";
    ok = expect(evaluator.expand("v##*b") == "c", "longest prefix chooses furthest match") && ok;
    ok = expect(evaluator.expand("v#*b") == "cabc", "shortest prefix chooses nearest match") && ok;
    ok = expect(evaluator.expand("v%%b*") == "a", "longest suffix chooses furthest match") && ok;
    ok = expect(evaluator.expand("v%b*") == "abca", "shortest suffix chooses nearest match") && ok;
    ok = expect(evaluator.expand("v##z*") == value, "unmatched longest prefix retains value") && ok;
    ok = expect(evaluator.expand("v%%*z") == value, "unmatched longest suffix retains value") && ok;
    value.clear();
    ok = expect(evaluator.expand("v-fallback").empty(), "empty binding is set") && ok;
    ok = expect(evaluator.expand("v:-fallback") == "fallback",
                "colon default treats empty as null") &&
         ok;
    present = false;
    ok = expect(evaluator.expand("v-fallback") == "fallback", "unset binding uses default") && ok;
    return ok;
}
}  // namespace

int main() {
    cjsh_env::reset_shell_state();
    config::interactive_mode = false;
    config::force_interactive = false;
    g_shell = std::make_unique<Shell>();
    g_shell->set_interactive_mode(false);
    const bool lookup_ok = test_variable_presence_and_scope();
    const bool expansion_ok = test_parameter_expansion_work();
    g_shell.reset();
    if (!lookup_ok || !expansion_ok) {
        return 1;
    }
    std::puts("All 2 variable lookup and expansion tests passed");
    return 0;
}
