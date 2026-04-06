/*
  approot_command.cpp

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

#include "approot_command.h"

#include "builtin_help.h"

#include <iostream>
#include <optional>

#include "cd_command.h"
#include "cjsh_filesystem.h"
#include "error_out.h"

namespace {

const char* kApprootUsage = "Usage: approot [-p|--print] [TARGET]";
const char* kApprootTargetsSummary =
    "Targets: config (default), cache, completions, env/cjshenv, profile/cjprofile, "
    "rc/cjshrc, logout/cjlogout, home, cjsh.";
const char* kApprootValidTargets =
    "Valid targets: config, cache, completions, env, cjshenv, profile, cjprofile, rc, "
    "cjshrc, logout, cjlogout, home, cjsh";

std::optional<std::filesystem::path> resolve_approot_target(const std::string& target) {
    if (target == "config") {
        return cjsh_filesystem::g_cjsh_config_path();
    }

    if (target == "cache") {
        return cjsh_filesystem::g_cjsh_cache_path();
    }

    if (target == "completions") {
        return cjsh_filesystem::g_cjsh_generated_completions_path();
    }

    if (target == "env" || target == "cjshenv") {
        return cjsh_filesystem::g_cjsh_env_path().parent_path();
    }

    if (target == "profile" || target == "cjprofile") {
        return cjsh_filesystem::g_cjsh_profile_path().parent_path();
    }

    if (target == "rc" || target == "cjshrc") {
        return cjsh_filesystem::g_cjsh_source_path().parent_path();
    }

    if (target == "logout" || target == "cjlogout") {
        return cjsh_filesystem::g_cjsh_logout_path().parent_path();
    }

    if (target == "home") {
        return cjsh_filesystem::g_user_home_path();
    }

    if (target == "cjsh") {
        return std::filesystem::path(cjsh_filesystem::resolve_cjsh_executable_directory());
    }

    return std::nullopt;
}

}  // namespace

int approot_command(const std::vector<std::string>& args, std::string& current_directory,
                    std::string& previous_directory, Shell* shell) {
    if (builtin_handle_help(args,
                            {kApprootUsage, "Change to a cjsh application directory.",
                             "Use -p/--print to print the resolved directory without changing it.",
                             "Generated-file targets jump to the directory containing the file.",
                             kApprootTargetsSummary})) {
        return 0;
    }

    bool print_only = false;
    std::optional<std::string> target;
    bool positional_only = false;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& token = args[i];

        if (!positional_only && (token == "-p" || token == "--print")) {
            print_only = true;
            continue;
        }

        if (!positional_only && token == "--") {
            positional_only = true;
            continue;
        }

        if (!positional_only && !token.empty() && token.front() == '-') {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "approot",
                         "invalid option: " + token,
                         {kApprootUsage}});
            return 2;
        }

        if (target.has_value()) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "approot", "too many arguments", {kApprootUsage}});
            return 2;
        }

        target = token;
    }

    std::string target_name = target.value_or("config");
    auto destination = resolve_approot_target(target_name);
    if (!destination.has_value()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "approot",
                     "unknown target '" + target_name + "'",
                     {kApprootValidTargets, kApprootUsage}});
        return 2;
    }

    if (print_only) {
        std::cout << destination->string() << '\n';
        return 0;
    }

    return change_directory(destination->string(), current_directory, previous_directory, shell);
}
