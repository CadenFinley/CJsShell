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

#include <array>
#include <optional>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include "cd_command.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell_env.h"

namespace {

const char* kApprootUsage = "Usage: approot [TARGET]";
const char* kApprootTargetsSummary =
    "Targets: config (default), cache, completions, env/cjshenv, profile/cjprofile, "
    "rc/cjshrc, logout/cjlogout, home, cjsh.";
const char* kApprootValidTargets =
    "Valid targets: config, cache, completions, env, cjshenv, profile, cjprofile, rc, "
    "cjshrc, logout, cjlogout, home, cjsh";

std::filesystem::path normalize_path(const std::filesystem::path& raw_path) {
    if (raw_path.empty()) {
        return raw_path;
    }

    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::canonical(raw_path, ec);
    if (!ec && !canonical.empty()) {
        return canonical;
    }

    ec.clear();
    std::filesystem::path weakly_canonical_path = std::filesystem::weakly_canonical(raw_path, ec);
    if (!ec && !weakly_canonical_path.empty()) {
        return weakly_canonical_path;
    }

    return raw_path.lexically_normal();
}

std::optional<std::filesystem::path> resolve_running_executable_path() {
#if defined(__APPLE__)
    uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    if (size == 0) {
        return std::nullopt;
    }

    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0 || buffer.empty()) {
        return std::nullopt;
    }

    return std::filesystem::path(buffer.data());
#elif defined(__linux__)
    std::array<char, 4096> buffer{};
    ssize_t bytes_read = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (bytes_read <= 0) {
        return std::nullopt;
    }

    buffer[static_cast<size_t>(bytes_read)] = '\0';
    return std::filesystem::path(buffer.data());
#else
    return std::nullopt;
#endif
}

std::optional<std::filesystem::path> resolve_executable_token(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }

    std::filesystem::path candidate(token);
    if (!candidate.is_absolute()) {
        std::string resolved = cjsh_filesystem::find_executable_in_path(token);
        if (!resolved.empty()) {
            candidate = std::filesystem::path(resolved);
        } else {
            std::error_code ec;
            std::filesystem::path absolute_candidate = std::filesystem::absolute(candidate, ec);
            if (!ec) {
                candidate = absolute_candidate;
            }
        }
    }

    std::error_code exists_ec;
    if (!std::filesystem::exists(candidate, exists_ec) || exists_ec) {
        return std::nullopt;
    }

    std::error_code file_ec;
    if (std::filesystem::is_directory(candidate, file_ec) || file_ec) {
        return std::nullopt;
    }

    return normalize_path(candidate);
}

std::filesystem::path resolve_cjsh_executable_directory() {
    if (auto executable_path = resolve_running_executable_path(); executable_path.has_value()) {
        std::filesystem::path normalized = normalize_path(*executable_path);
        if (!normalized.empty() && !normalized.parent_path().empty()) {
            return normalized.parent_path();
        }
    }

    for (const char* variable_name : {"0", "SHELL"}) {
        std::string value = cjsh_env::get_shell_variable_value(variable_name);
        if (auto resolved = resolve_executable_token(value); resolved.has_value()) {
            if (!resolved->parent_path().empty()) {
                return resolved->parent_path();
            }
        }
    }

    std::string from_path = cjsh_filesystem::find_executable_in_path("cjsh");
    if (!from_path.empty()) {
        std::filesystem::path normalized = normalize_path(from_path);
        if (!normalized.parent_path().empty()) {
            return normalized.parent_path();
        }
    }

    return std::filesystem::path(cjsh_filesystem::safe_current_directory());
}

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
        return resolve_cjsh_executable_directory();
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
