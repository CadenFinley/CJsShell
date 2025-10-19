#include "cjshopt_command.h"

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"

namespace {
int handle_generate_command_common(const std::vector<std::string>& args,
                                   const std::string& command_name,
                                   const cjsh_filesystem::fs::path& target_path,
                                   const std::string& description,
                                   const std::function<bool()>& generator) {
    static const std::vector<std::string> base_usage = {
        "Options:", "  -f, --force   Overwrite the existing file if it exists"};

    bool force = false;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& option = args[i];
        if (option == "--help" || option == "-h") {
            if (!g_startup_active) {
                std::cout << "Usage: " << command_name << " [--force]\n";
                std::cout << description << "\n";
                for (const auto& line : base_usage) {
                    std::cout << line << '\n';
                }
            }
            return 0;
        }

        if (option == "--force" || option == "-f") {
            force = true;
            continue;
        }

        print_error({ErrorType::INVALID_ARGUMENT,
                     command_name,
                     "Unknown option '" + option + "'",
                     {"Use --help to view available options"}});
        return 1;
    }

    bool file_exists = cjsh_filesystem::fs::exists(target_path);
    if (file_exists && !force) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     command_name,
                     "File already exists at '" + target_path.string() + "'",
                     {"Pass --force to overwrite the existing file"}});
        return 1;
    }

    if (!generator()) {
        return 1;
    }

    if (!g_startup_active) {
        std::cout << (file_exists ? "Updated" : "Created") << " " << target_path << '\n';
    }

    return 0;
}
}  // namespace

int generate_profile_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(args, "generate-profile",
                                          cjsh_filesystem::g_cjsh_profile_path,
                                          "Create a default ~/.cjprofile configuration file.",
                                          []() { return cjsh_filesystem::create_profile_file(); });
}

int generate_rc_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(args, "generate-rc", cjsh_filesystem::g_cjsh_source_path,
                                          "Create a default ~/.cjshrc configuration file.",
                                          []() { return cjsh_filesystem::create_source_file(); });
}

int generate_logout_command(const std::vector<std::string>& args) {
    return handle_generate_command_common(args, "generate-logout",
                                          cjsh_filesystem::g_cjsh_logout_path,
                                          "Create a default ~/.cjsh_logout file.",
                                          []() { return cjsh_filesystem::create_logout_file(); });
}
