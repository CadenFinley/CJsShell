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
#include <system_error>

#include "cd_command.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell_env.h"

namespace {

const char* kApprootUsage = "Usage: approot [-p|--print] [-f|--file] [TARGET]";
const char* kApprootTargetsSummary =
    "Targets: config (default), cache, history, firstboot/first_boot, completions, "
    "env/cjshenv, profile/cjprofile, rc/cjshrc, logout/cjlogout, home, cjsh.";
const char* kApprootValidTargets =
    "Valid targets: config, cache, history, firstboot, first_boot, completions, env, "
    "cjshenv, profile, cjprofile, rc, cjshrc, logout, cjlogout, home, cjsh";

std::filesystem::path parent_directory_or_self(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    return parent.empty() ? path : parent;
}

std::filesystem::path normalize_target_path(std::filesystem::path path) {
    std::error_code abs_ec;
    if (!path.is_absolute()) {
        auto absolute_path = std::filesystem::absolute(path, abs_ec);
        if (!abs_ec) {
            path = absolute_path;
        }
    }
    return path.lexically_normal();
}

bool startup_target_exists(const std::filesystem::path& candidate, bool require_regular_file) {
    if (require_regular_file) {
        std::error_code status_ec;
        auto status = std::filesystem::status(candidate, status_ec);
        return !status_ec && std::filesystem::is_regular_file(status);
    }

    std::error_code exists_ec;
    return std::filesystem::exists(candidate, exists_ec) && !exists_ec;
}

std::filesystem::path resolve_startup_target_file_path(const std::filesystem::path& primary,
                                                       const std::filesystem::path& alternate,
                                                       bool require_regular_file) {
    if (startup_target_exists(primary, require_regular_file)) {
        return primary;
    }

    if (startup_target_exists(alternate, require_regular_file)) {
        return alternate;
    }

    return primary;
}

std::filesystem::path resolve_env_target_file_path() {
    if (cjsh_env::shell_variable_is_set("CJSH_ENV")) {
        std::string env_override = cjsh_env::get_shell_variable_value("CJSH_ENV");
        if (!env_override.empty()) {
            return normalize_target_path(std::filesystem::path(env_override));
        }
    }

    return resolve_startup_target_file_path(cjsh_filesystem::g_cjsh_env_path(),
                                            cjsh_filesystem::g_cjsh_env_alt_path(), true);
}

struct ApprootTargetPath {
    std::filesystem::path directory_path;
    std::optional<std::filesystem::path> file_path;
};

ApprootTargetPath make_dir_target(const std::filesystem::path& directory_path) {
    return {directory_path, std::nullopt};
}

ApprootTargetPath make_file_target(const std::filesystem::path& file_path) {
    return {parent_directory_or_self(file_path), file_path};
}

std::optional<ApprootTargetPath> resolve_approot_target(const std::string& target) {
    if (target == "config") {
        return make_dir_target(cjsh_filesystem::g_cjsh_config_path());
    }

    if (target == "cache") {
        return make_dir_target(cjsh_filesystem::g_cjsh_cache_path());
    }

    if (target == "history") {
        return make_file_target(cjsh_filesystem::g_cjsh_history_path());
    }

    if (target == "firstboot" || target == "first_boot") {
        return make_file_target(cjsh_filesystem::g_cjsh_first_boot_path());
    }

    if (target == "completions") {
        return make_dir_target(cjsh_filesystem::g_cjsh_generated_completions_path());
    }

    if (target == "env" || target == "cjshenv") {
        return make_file_target(resolve_env_target_file_path());
    }

    if (target == "profile" || target == "cjprofile") {
        return make_file_target(
            resolve_startup_target_file_path(cjsh_filesystem::g_cjsh_profile_path(),
                                             cjsh_filesystem::g_cjsh_profile_alt_path(), false));
    }

    if (target == "rc" || target == "cjshrc") {
        return make_file_target(
            resolve_startup_target_file_path(cjsh_filesystem::g_cjsh_source_path(),
                                             cjsh_filesystem::g_cjsh_source_alt_path(), false));
    }

    if (target == "logout" || target == "cjlogout") {
        return make_file_target(
            resolve_startup_target_file_path(cjsh_filesystem::g_cjsh_logout_path(),
                                             cjsh_filesystem::g_cjsh_logout_alt_path(), false));
    }

    if (target == "home") {
        return make_dir_target(cjsh_filesystem::g_user_home_path());
    }

    if (target == "cjsh") {
        const std::filesystem::path executable_path =
            normalize_target_path(cjsh_filesystem::resolve_cjsh_executable_path());
        return ApprootTargetPath{parent_directory_or_self(executable_path), executable_path};
    }

    return std::nullopt;
}

}  // namespace

int approot_command(const std::vector<std::string>& args, std::string& current_directory,
                    std::string& previous_directory, Shell* shell) {
    if (builtin_handle_help(args,
                            {kApprootUsage, "Change to a cjsh application directory.",
                             "Use -p/--print to print the resolved directory without changing it.",
                             "Use -f/--file to print file-backed target paths (implies --print).",
                             "Generated-file targets jump to the directory containing the file.",
                             kApprootTargetsSummary})) {
        return 0;
    }

    bool print_only = false;
    bool file_path_mode = false;
    std::optional<std::string> target;
    bool positional_only = false;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& token = args[i];

        if (!positional_only && (token == "-p" || token == "--print")) {
            print_only = true;
            continue;
        }

        if (!positional_only && (token == "-f" || token == "--file")) {
            file_path_mode = true;
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

    std::filesystem::path resolved_path = destination->directory_path;
    if (file_path_mode && destination->file_path.has_value()) {
        resolved_path = destination->file_path.value();
    }

    if (print_only) {
        std::cout << resolved_path.string() << '\n';
        return 0;
    }

    return change_directory(resolved_path.string(), current_directory, previous_directory, shell);
}
