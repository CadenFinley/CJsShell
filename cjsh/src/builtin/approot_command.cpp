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

#include <optional>

#include "cd_command.h"
#include "cjsh_filesystem.h"
#include "error_out.h"

namespace {

const char* kApprootUsage = "Usage: approot [TARGET]";
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
                            {"Usage: approot [TARGET]", "Change to a cjsh application directory.",
                             "Generated-file targets jump to the directory containing the file.",
                             kApprootTargetsSummary})) {
        return 0;
    }

    if (args.size() > 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "approot", "too many arguments", {kApprootUsage}});
        return 2;
    }

    std::string target = args.size() == 2 ? args[1] : "config";
    auto destination = resolve_approot_target(target);
    if (!destination.has_value()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "approot",
                     "unknown target '" + target + "'",
                     {kApprootValidTargets, kApprootUsage}});
        return 2;
    }

    return change_directory(destination->string(), current_directory, previous_directory, shell);
}
