/*
  test_command_lookup.cpp

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

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "cjsh_filesystem.h"
#include "command_command.h"
#include "shell.h"
#include "shell_env.h"
#include "status_line.h"
#include "suggestion_utils.h"
#include "type_which_command.h"

std::unique_ptr<Shell> g_shell;

namespace {
namespace fs = std::filesystem;

bool expect(bool condition, const char* message) {
    if (!condition) {
        (void)std::fprintf(stderr, "[FAIL] %s\n", message);
    }
    return condition;
}

void executable(const fs::path& path) {
    std::ofstream(path) << "#!/bin/sh\nexit 0\n";
    fs::permissions(path, fs::perms::owner_all);
}

struct BuiltinResult {
    int status;
    std::string output;
};

BuiltinResult run_builtin(int (*command)(const std::vector<std::string>&, Shell*),
                          const std::vector<std::string>& args) {
    std::ostringstream output;
    auto* previous = std::cout.rdbuf(output.rdbuf());
    const int status = command(args, g_shell.get());
    std::cout.rdbuf(previous);
    return {status, output.str()};
}

bool suggests(const std::string& query, const std::string& command) {
    const auto suggestions = suggestion_utils::generate_command_suggestions(query);
    return std::find(suggestions.begin(), suggestions.end(), "Did you mean '" + command + "'?") !=
           suggestions.end();
}

bool test_suggestions(const fs::path& root) {
    executable(root / "lookuptool");
    executable(root / "unrelated-executable-in-path");
    std::ofstream(root / "lookupplain") << "plain\n";
    std::ofstream(root / "echo") << "plain\n";
    fs::create_directory(root / "lookupdirectory");
    fs::create_symlink(root / "lookuptool", root / "lookuplink");
    fs::create_symlink(root / "absent", root / "lookupbroken");
    cjsh_filesystem::reset_path_hash();

    bool ok =
        expect(suggests("lookuptol", "lookuptool"), "suggestions find nearby executable names");
    ok =
        expect(suggests("lookuplnk", "lookuplink"), "suggestions follow executable symlinks") && ok;
    ok = expect(!suggests("lookupplai", "lookupplain") &&
                    !suggests("lookupdirectry", "lookupdirectory") &&
                    !suggests("lookupbrokn", "lookupbroken"),
                "suggestions reject nonexecutables, directories, and broken links") &&
         ok;
    ok = expect(suggests("ech", "echo"),
                "a nonexecutable PATH file must not hide a builtin suggestion") &&
         ok;
    const auto entries = cjsh_filesystem::get_path_hash_entries();
    ok = expect(std::none_of(entries.begin(), entries.end(),
                             [](const auto& entry) {
                                 return entry.command == "unrelated-executable-in-path";
                             }),
                "suggestions must not eagerly hash unrelated PATH executables") &&
         ok;

    g_shell->set_aliases({{"lookupplain", "echo alias"}});
    g_shell->set_abbreviations({{"lookupbroken", "echo abbreviation"}});
    ok = expect(g_shell->execute("lookupfunction() { :; }") == 0,
                "create a function for suggestion lookup") &&
         ok;
    ok = expect(suggests("lookupplai", "lookupplain") && suggests("lookupbrokn", "lookupbroken") &&
                    suggests("lookupfunctio", "lookupfunction"),
                "aliases, abbreviations, and functions remain valid suggestion sources") &&
         ok;
    g_shell->set_aliases({});
    g_shell->set_abbreviations({});
    return ok;
}

bool test_builtin_resolution(const fs::path& root) {
    executable(root / "echo");
    executable(root / "cjshopt");
    executable(root / "lookupfunction");
    g_shell->set_aliases({{"lookupalias", "echo alias"}});
    cjsh_filesystem::reset_path_hash();
    bool ok = true;
    for (const auto& args : std::vector<std::vector<std::string>>{{"type", "echo"},
                                                                  {"type", "if"},
                                                                  {"type", "lookupalias"},
                                                                  {"type", "lookupfunction"},
                                                                  {"type", "-t", "echo"}}) {
        ok = expect(run_builtin(type_command, args).status == 0,
                    "type resolves shell commands before searching PATH") &&
             ok;
    }
    ok = expect(run_builtin(command_command, {"command", "-v", "cjshopt"}).output == "cjshopt\n",
                "command -v preserves builtin output") &&
         ok;
    ok = expect(run_builtin(which_command, {"which", "echo"}).output ==
                    "echo is a cjsh builtin (custom implementation)\n",
                "which preserves custom builtin precedence") &&
         ok;
    ok = expect(cjsh_filesystem::get_path_hash_entries().empty(),
                "shell-only answers must not resolve and hash external counterparts") &&
         ok;

    const auto all_types = run_builtin(type_command, {"type", "-a", "echo"});
    ok = expect(all_types.status == 0 &&
                    all_types.output.find("shell builtin") != std::string::npos &&
                    all_types.output.find((root / "echo").string()) != std::string::npos,
                "type -a still includes external counterparts") &&
         ok;
    ok = expect(run_builtin(type_command, {"type", "-P", "echo"}).output ==
                    (root / "echo").string() + "\n",
                "type -P still forces filesystem lookup") &&
         ok;
    ok = expect(run_builtin(type_command, {"type", "-f", "lookupfunction"}).output ==
                    "lookupfunction is " + (root / "lookupfunction").string() + "\n",
                "type -f still bypasses functions") &&
         ok;
    ok = expect(run_builtin(which_command, {"which", "lookupfunction"}).output ==
                    (root / "lookupfunction").string() + "\n",
                "which retains external precedence for ordinary names") &&
         ok;
    const auto all_which = run_builtin(which_command, {"which", "-a", "echo"});
    ok = expect(all_which.status == 0 &&
                    all_which.output.find((root / "echo").string()) != std::string::npos,
                "which -a still searches PATH for custom builtins") &&
         ok;
    g_shell->set_aliases({});
    return ok;
}

bool test_default_path_queries(const fs::path& root) {
    executable(root / "sh");
    bool ok = true;
    for (const auto& original : {std::optional<std::string>(root.string()),
                                 std::optional<std::string>(""), std::optional<std::string>()}) {
        if (original) {
            (void)cjsh_env::set_shell_variable_value("PATH", *original);
        } else {
            (void)cjsh_env::unset_shell_variable_value("PATH");
        }
        const auto result = run_builtin(command_command, {"command", "-pv", "sh"});
        ok = expect(result.status == 0 &&
                        (result.output == "/usr/bin/sh\n" || result.output == "/bin/sh\n"),
                    "command -pv searches the default PATH") &&
             ok;
        ok = expect(cjsh_env::shell_variable_is_set("PATH") == original.has_value() &&
                        (!original || cjsh_env::get_shell_variable_value("PATH") == *original),
                    "command -pv restores PATH, including empty and unset values") &&
             ok;
        const auto executed = run_builtin(command_command, {"command", "-p", ":"});
        ok = expect(executed.status == 0 &&
                        cjsh_env::shell_variable_is_set("PATH") == original.has_value() &&
                        (!original || cjsh_env::get_shell_variable_value("PATH") == *original),
                    "command -p execution also restores empty and unset PATH values") &&
             ok;
    }
    (void)cjsh_env::set_shell_variable_value("PATH", root.string());
    return ok;
}

bool test_status_lookup_scope(const fs::path& root) {
    config::status_line_enabled = true;
    config::status_reporting_enabled = true;
    cjsh_filesystem::reset_path_hash();
    // Populate the prompt's name snapshot before another process installs a command.
    (void)cjsh_filesystem::get_path_completion_candidates();
    executable(root / "lookupnew");
    status_line::clear_transient_status_message();
    const char* message = status_line::create_below_syntax_message("lookupnew ", nullptr);
    bool ok = expect(
        message && std::string(message).find("Unknown command: lookupnew") != std::string::npos,
        "status analysis shares the current prompt's PATH snapshot");
    const auto fresh = run_builtin(command_command, {"command", "-v", "lookupnew"});
    ok = expect(fresh.status == 0 && fresh.output == (root / "lookupnew").string() + "\n",
                "explicit queries see commands installed after the prompt snapshot") &&
         ok;
    cjsh_filesystem::reset_interactive_path_cache();
    status_line::clear_transient_status_message();
    message = status_line::create_below_syntax_message("lookupnew ", nullptr);
    ok = expect(!message || std::string(message).find("Unknown command") == std::string::npos,
                "status analysis recognizes new commands after a prompt refresh") &&
         ok;

    // A user callback must run outside the advisory scope, even though built-in analysis uses it.
    {
        const cjsh_filesystem::ScopedInteractivePathLookup scope;
        (void)cjsh_filesystem::find_executable_in_path("lookupcallback");
    }
    executable(root / "lookupcallback");
    ok = expect(g_shell->execute("lookupstatus() { command -v lookupcallback >/dev/null && "
                                 "CJSH_STATUS_OUTPUT=fresh; }") == 0,
                "create a status callback containing an explicit query") &&
         ok;
    status_line::set_user_status_callback_function("lookupstatus");
    message = status_line::create_below_syntax_message("echo callback", nullptr);
    ok = expect(message && std::string(message).find("fresh") != std::string::npos,
                "user status callbacks retain fresh explicit command queries") &&
         ok;
    status_line::clear_user_status_callback_function();
    return ok;
}
}  // namespace

int main() {
    char temporary[] = "/tmp/cjsh-command-lookup-XXXXXX";
    if (mkdtemp(temporary) == nullptr) {
        std::perror("mkdtemp");
        return 1;
    }
    const fs::path root(temporary);
    cjsh_env::reset_shell_state();
    cjsh_env::set_startup_active(false);
    config::interactive_mode = false;
    config::force_interactive = false;
    config::no_config = true;
    config::history_enabled = false;
    config::completion_learning_enabled = false;
    g_shell = std::make_unique<Shell>();
    g_shell->set_interactive_mode(false);
    (void)cjsh_env::set_shell_variable_value("PATH", root.string());

    const bool suggestions = test_suggestions(root);
    const bool builtins = test_builtin_resolution(root);
    const bool default_path = test_default_path_queries(root);
    const bool status = test_status_lookup_scope(root);
    g_shell.reset();
    fs::remove_all(root);
    if (!(suggestions && builtins && default_path && status)) {
        return 1;
    }
    (void)std::printf("All 4 command lookup tests passed\n");
    return 0;
}
