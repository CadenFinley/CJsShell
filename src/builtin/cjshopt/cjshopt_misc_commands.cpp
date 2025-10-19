#include "cjshopt_command.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bookmark_database.h"
#include "cjsh.h"
#include "cjsh_completions.h"
#include "error_out.h"
#include "isocline/isocline.h"
#include "token_constants.h"

namespace {
std::string resolve_style_registry_name(const std::string& token_type) {
    if (token_type.rfind("ic-", 0) == 0) {
        return token_type;
    }
    return "cjsh-" + token_type;
}
}  

int startup_flag_command(const std::vector<std::string>& args) {
    if (!g_startup_active) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "login-startup-arg",
                     "Startup flags can only be set in configuration files (e.g., ~/.cjprofile)",
                     {"To set startup flags, add 'cjshopt login-startup-arg ...' commands to your "
                      "~/.cjprofile file."}});
        return 1;
    }

    if (args.size() < 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT,
             "login-startup-arg",
             "Missing flag argument",
             {"Usage: login-startup-arg [--flag-name]",
              "Available flags:", "  --login              Set login mode",
              "  --interactive        Force interactive mode",
              "  --debug              Enable debug mode", "  --no-themes          Disable themes",
              "  --no-colors          Disable colors", "  --no-titleline       Disable title line",
              "  --show-startup-time  Display shell startup time",
              "  --no-source          Don't source the .cjshrc file",
              "  --no-completions     Disable tab completions",
              "  --no-syntax-highlighting Disable syntax highlighting",
              "  --no-smart-cd        Disable smart cd functionality",
              "  --no-prompt          Use simple '#' prompt instead of themed prompt",
              R"(  --minimal            Disable all unique cjsh features (themes, colors, completions, syntax highlighting, smart cd, sourcing, custom ls, startup time display))",
              R"(  --disable-custom-ls  Use system ls command instead of builtin ls)",
              "  --startup-test       Enable startup test mode"}});
        return 1;
    }

    const std::string& flag = args[1];

    if (flag == "--login" || flag == "--interactive" || flag == "--debug" ||
        flag == "--no-prompt" || flag == "--no-themes" || flag == "--no-colors" ||
        flag == "--no-titleline" || flag == "--show-startup-time" || flag == "--no-source" ||
        flag == "--no-completions" || flag == "--no-syntax-highlighting" ||
        flag == "--no-smart-cd" || flag == "--minimal" || flag == "--startup-test" ||
        flag == "--disable-custom-ls") {
        bool flag_exists = false;
        for (const auto& existing_flag : g_profile_startup_args) {
            if (existing_flag == flag) {
                flag_exists = true;
                break;
            }
        }

        if (!flag_exists) {
            g_profile_startup_args.push_back(flag);
        }
    } else {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "login-startup-arg", "unknown flag '" + flag + "'", {}});
        return 1;
    }

    return 0;
}

int style_def_command(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        if (!g_startup_active) {
            std::cout << "Usage: style_def <token_type> <style>\n\n";
            std::cout << "Define or redefine a syntax highlighting style.\n\n";
            std::cout << "Token types:\n";
            for (const auto& pair : token_constants::default_styles) {
                std::cout << "  " << pair.first << " (default: " << pair.second << ")\n";
            }
            std::cout << "\nStyle format: [bold] [italic] [underline] color=#RRGGBB|color=name\n";
            std::cout << "Color names: red, green, blue, yellow, magenta, cyan, white, black\n";
            std::cout << "ANSI colors: ansi-black, ansi-red, ansi-green, ansi-yellow, etc.\n\n";
            std::cout << "Examples:\n";
            std::cout << "  style_def builtin \"bold color=#FFB86C\"\n";
            std::cout << "  style_def system \"color=#50FA7B\"\n";
            std::cout << "  style_def installed \"color=#8BE9FD\"\n";
            std::cout << "  style_def comment \"italic color=green\"\n";
            std::cout << "  style_def string \"color=#F1FA8C\"\n\n";
            std::cout << "To reset all styles to defaults, use: style_def --reset\n";
        }
        return 0;
    }

    if (args.size() == 2 && args[1] == "--reset") {
        for (const auto& pair : token_constants::default_styles) {
            ic_style_def(pair.first.c_str(), pair.second.c_str());
        }
        if (!g_startup_active) {
            std::cout << "All syntax highlighting styles reset to defaults.\n";
        }
        return 0;
    }

    if (args.size() != 3) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "style_def",
                     "expected 2 arguments: <token_type> <style>",
                     {"Use 'style_def' to see available token types"}});
        return 1;
    }

    const std::string& token_type = args[1];
    const std::string& style = args[2];

    if (token_constants::default_styles.find(token_type) == token_constants::default_styles.end()) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "style_def",
                     "unknown token type: " + token_type,
                     {"Use 'style_def' to see available token types"}});
        return 1;
    }

    apply_custom_style(token_type, style);

    return 0;
}

void apply_custom_style(const std::string& token_type, const std::string& style) {
    std::string full_style_name = resolve_style_registry_name(token_type);
    ic_style_def(full_style_name.c_str(), style.c_str());
}

int set_max_bookmarks_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: set-max-bookmarks <number>",
        "",
        "Set the maximum number of directory bookmarks to store.",
        "Default is 100. Minimum is 10. Maximum is 1000.",
        "",
        "Example:",
        "  set-max-bookmarks 200"};

    if (args.size() == 1) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "expected 1 argument: <number>", usage_lines});
        return 1;
    }

    if (args.size() > 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "too many arguments provided", usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    if (option == "--help" || option == "-h") {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    int number = 0;
    try {
        number = std::stoi(option);
    } catch (const std::invalid_argument&) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks", "invalid number: " + option,
                     usage_lines});
        return 1;
    } catch (const std::out_of_range&) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "number out of range: " + option, usage_lines});
        return 1;
    }

    if (number < 10 || number > 1000) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-max-bookmarks",
                     "number must be between 10 and 1000", usage_lines});
        return 1;
    }

    bookmark_database::g_bookmark_db.set_max_bookmarks(number);
    if (!g_startup_active) {
        std::cout << "Maximum bookmarks set to "
                  << bookmark_database::g_bookmark_db.get_max_bookmarks() << ".\n";
    }

    return 0;
}

int set_history_max_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: set-history-max <number|default|status>",
        "",
        "Configure the maximum number of entries written to the history file.",
        "Use 0 to disable history persistence entirely.",
        "Use 'default' to restore the built-in limit (" +
            std::to_string(get_history_default_history_limit()) + " entries).",
        "Use 'status' to view the current setting.",
        "Valid range: " + std::to_string(get_history_min_history_limit()) + " - " +
            std::to_string(get_history_max_history_limit()) + "."};

    if (args.size() == 1) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        print_error(
            {ErrorType::INVALID_ARGUMENT, "set-history-max", "expected 1 argument", usage_lines});
        return 1;
    }

    if (args.size() > 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-history-max", "too many arguments provided",
                     usage_lines});
        return 1;
    }

    const std::string& option = args[1];
    std::string normalized = option;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (normalized == "--help" || normalized == "-h") {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    if (normalized == "status" || normalized == "--status") {
        if (!g_startup_active) {
            long current_limit = get_history_max_entries();
            if (current_limit <= 0) {
                std::cout << "History persistence is currently disabled.\n";
            } else {
                std::cout << "History file retains up to " << current_limit << " entries." << '\n';
            }
        }
        return 0;
    }

    long requested_limit = 0;
    if (normalized == "default" || normalized == "--default") {
        requested_limit = get_history_default_history_limit();
    } else {
        try {
            requested_limit = std::stol(option);
        } catch (const std::invalid_argument&) {
            print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                         "invalid number: " + option, usage_lines});
            return 1;
        } catch (const std::out_of_range&) {
            print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                         "number out of range: " + option, usage_lines});
            return 1;
        }
    }

    if (requested_limit < get_history_min_history_limit()) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                     "value must be greater than or equal to " +
                         std::to_string(get_history_min_history_limit()),
                     usage_lines});
        return 1;
    }

    if (requested_limit > get_history_max_history_limit()) {
        print_error({ErrorType::INVALID_ARGUMENT, "set-history-max",
                     "value exceeds the maximum allowed: " +
                         std::to_string(get_history_max_history_limit()),
                     usage_lines});
        return 1;
    }

    std::string error_message;
    if (!set_history_max_entries(requested_limit, &error_message)) {
        if (error_message.empty()) {
            error_message = "Failed to update history limit.";
        }
        print_error({ErrorType::RUNTIME_ERROR, "set-history-max", error_message, {}});
        return 1;
    }

    if (!g_startup_active) {
        long applied_limit = get_history_max_entries();
        if (applied_limit <= 0) {
            std::cout << "History persistence disabled.\n";
        } else {
            std::cout << "History file will retain up to " << applied_limit << " entries." << '\n';
        }
    }

    return 0;
}

int bookmark_blacklist_command(const std::vector<std::string>& args) {
    static const std::vector<std::string> usage_lines = {
        "Usage: bookmark-blacklist <subcommand> [path]",
        "",
        "Manage directories that cannot be bookmarked.",
        "",
        "Subcommands:",
        "  add <path>      Add a directory to the blacklist",
        "  remove <path>   Remove a directory from the blacklist",
        "  list            List all blacklisted directories",
        "  clear           Clear the entire blacklist",
        "",
        "Examples:",
        "  cjshopt bookmark-blacklist add /tmp",
        "  cjshopt bookmark-blacklist remove /tmp",
        "  cjshopt bookmark-blacklist list",
        "  cjshopt bookmark-blacklist clear"};

    if (args.size() < 2) {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        print_error({ErrorType::INVALID_ARGUMENT, "bookmark-blacklist", "expected a subcommand",
                     usage_lines});
        return 1;
    }

    const std::string& subcommand = args[1];

    if (subcommand == "--help" || subcommand == "-h") {
        if (!g_startup_active) {
            for (const auto& line : usage_lines) {
                std::cout << line << '\n';
            }
        }
        return 0;
    }

    if (subcommand == "add") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT, "bookmark-blacklist add",
                         "expected a path argument", usage_lines});
            return 1;
        }

        const std::string& path = args[2];

        std::vector<std::string> affected_bookmarks;
        if (!g_startup_active) {
            try {
                std::filesystem::path fs_path(path);
                std::string canonical_path;

                if (std::filesystem::exists(fs_path)) {
                    canonical_path = std::filesystem::canonical(fs_path).string();
                } else {
                    canonical_path = std::filesystem::absolute(fs_path).string();
                }

                auto all_bookmarks = bookmark_database::get_directory_bookmarks();
                for (const auto& [name, bookmark_path] : all_bookmarks) {
                    if (bookmark_path == canonical_path) {
                        affected_bookmarks.push_back(name);
                    }
                }
            } catch (...) {
            }
        }

        auto result = bookmark_database::add_path_to_blacklist(path);

        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR, "bookmark-blacklist add", result.error(), {}});
            return 1;
        }

        if (!g_startup_active) {
            std::cout << "Added to blacklist: " << path << '\n';
            if (!affected_bookmarks.empty()) {
                std::cout << "Removed " << affected_bookmarks.size() << " existing bookmark(s): ";
                for (size_t i = 0; i < affected_bookmarks.size(); ++i) {
                    if (i > 0) {
                        std::cout << ", ";
                    }
                    std::cout << affected_bookmarks[i];
                }
                std::cout << '\n';
            }
        }
        return 0;

    } else if (subcommand == "remove") {
        if (args.size() < 3) {
            print_error({ErrorType::INVALID_ARGUMENT, "bookmark-blacklist remove",
                         "expected a path argument", usage_lines});
            return 1;
        }

        const std::string& path = args[2];
        auto result = bookmark_database::remove_path_from_blacklist(path);

        if (result.is_error()) {
            print_error(
                {ErrorType::RUNTIME_ERROR, "bookmark-blacklist remove", result.error(), {}});
            return 1;
        }

        if (!g_startup_active) {
            std::cout << "Removed from blacklist: " << path << '\n';
        }
        return 0;

    } else if (subcommand == "list") {
        auto blacklist = bookmark_database::get_bookmark_blacklist();

        if (blacklist.empty()) {
            if (!g_startup_active) {
                std::cout << "No directories are currently blacklisted.\n";
            }
        } else {
            if (!g_startup_active) {
                std::cout << "Blacklisted directories:\n";
                for (const auto& path : blacklist) {
                    std::cout << "  " << path << '\n';
                }
            }
        }
        return 0;

    } else if (subcommand == "clear") {
        auto result = bookmark_database::clear_bookmark_blacklist();

        if (result.is_error()) {
            print_error({ErrorType::RUNTIME_ERROR, "bookmark-blacklist clear", result.error(), {}});
            return 1;
        }

        if (!g_startup_active) {
            std::cout << "Blacklist cleared.\n";
        }
        return 0;

    } else {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "bookmark-blacklist",
                     "unknown subcommand '" + subcommand + "'",
                     {"Available subcommands: add, remove, list, clear"}});
        return 1;
    }

    return 0;
}
