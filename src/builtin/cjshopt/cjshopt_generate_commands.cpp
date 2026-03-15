/*
  cjshopt_generate_commands.cpp

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

#include "cjshopt_command.h"

#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "shell_env.h"

namespace {
int handle_generate_command_common(
    const std::vector<std::string>& args, const std::string& command_name,
    const std::filesystem::path& default_target_path,
    const std::optional<std::filesystem::path>& alternate_target_path,
    const std::string& description,
    const std::function<bool(const std::filesystem::path&)>& generator) {
    bool force = false;
    bool use_alternate = false;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& option = args[i];
        if (option == "--help" || option == "-h") {
            if (!cjsh_env::startup_active()) {
                std::string usage = "Usage: " + command_name + " [--force]";
                if (alternate_target_path) {
                    usage.append(" [--alt]");
                }
                std::cout << usage << '\n';
                std::cout << description << "\n";
                std::cout << "Default location: " << default_target_path << '\n';
                if (alternate_target_path) {
                    std::cout << "Alternate location (--alt): " << *alternate_target_path << '\n';
                }
                std::vector<std::string> usage_lines = {
                    "Options:", "  -f, --force   Overwrite the existing file if it exists"};
                if (alternate_target_path) {
                    usage_lines.emplace_back(
                        "  --alt        Write to the alternate configuration path");
                }
                for (const auto& line : usage_lines) {
                    std::cout << line << '\n';
                }
            }
            return 0;
        }

        if (option == "--force" || option == "-f") {
            force = true;
            continue;
        }

        if (option == "--alt") {
            if (!alternate_target_path) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             command_name,
                             "Option '--alt' is not supported for this command",
                             {"Use --help to view available options"}});
                return 1;
            }
            use_alternate = true;
            continue;
        }

        print_error({ErrorType::INVALID_ARGUMENT,
                     command_name,
                     "Unknown option '" + option + "'",
                     {"Use --help to view available options"}});
        return 1;
    }

    const std::filesystem::path& target_path =
        use_alternate ? alternate_target_path.value() : default_target_path;

    bool file_exists = std::filesystem::exists(target_path);
    if (file_exists && !force) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     command_name,
                     "File already exists at '" + target_path.string() + "'",
                     {"Pass --force to overwrite the existing file"}});
        return 1;
    }

    if (!generator(target_path)) {
        return 1;
    }

    if (!cjsh_env::startup_active()) {
        std::cout << (file_exists ? "Updated" : "Created") << " " << target_path << '\n';
    }

    return 0;
}
}  // namespace

int generate_profile_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(
        args, "generate-profile", cjsh_filesystem::g_cjsh_profile_path(),
        std::optional<std::filesystem::path>{cjsh_filesystem::g_cjsh_profile_alt_path()},
        "Create a default ~/.cjprofile configuration file.",
        [](const std::filesystem::path& target) {
            return cjsh_filesystem::create_profile_file(target);
        });
}

int generate_env_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(
        args, "generate-env", cjsh_filesystem::g_cjsh_env_path(),
        std::optional<std::filesystem::path>{cjsh_filesystem::g_cjsh_env_alt_path()},
        "Create a default ~/.cjshenv configuration file.", [](const std::filesystem::path& target) {
            return cjsh_filesystem::create_env_file(target);
        });
}

int generate_rc_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(
        args, "generate-rc", cjsh_filesystem::g_cjsh_source_path(),
        std::optional<std::filesystem::path>{cjsh_filesystem::g_cjsh_source_alt_path()},
        "Create a default ~/.cjshrc configuration file.", [](const std::filesystem::path& target) {
            return cjsh_filesystem::create_source_file(target);
        });
}

int generate_logout_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(
        args, "generate-logout", cjsh_filesystem::g_cjsh_logout_path(),
        std::optional<std::filesystem::path>{cjsh_filesystem::g_cjsh_logout_alt_path()},
        "Create a default ~/.cjlogout file.", [](const std::filesystem::path& target) {
            return cjsh_filesystem::create_logout_file(target);
        });
}
