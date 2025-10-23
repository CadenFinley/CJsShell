#include "cjshopt_command.h"

#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "error_out.h"

namespace {
void print_cjshopt_usage() {
    std::cout << "Usage: cjshopt <subcommand> [options]\n";
    std::cout << "Available subcommands:\n";
    std::cout
        << "  style_def <token_type> <style>   Define or redefine a syntax highlighting style\n";
    std::cout << "  login-startup-arg [--flag-name]  Add a startup flag (config file only)\n";
    std::cout << "  completion-case <on|off|status>  Configure completion case sensitivity "
                 "(default: enabled)\n";
    std::cout << "  completion-spell <on|off|status> Configure completion spell correction "
                 "(default: enabled)\n";
    std::cout << "  line-numbers <on|off|relative|absolute|status>    Configure line numbers in "
                 "multiline input (default: enabled)\n";
    std::cout << "  current-line-number-highlight <on|off|status>    Configure current line number "
                 "highlighting (default: enabled)\n";
    std::cout << "  multiline-start-lines <count|status> Configure default multiline prompt "
                 "height (default: 1)\n";
    std::cout << "  hint-delay <milliseconds>        Set hint display delay in milliseconds\n";
    std::cout
        << "  completion-preview <on|off|status> Configure completion preview (default: enabled)\n";
    std::cout << "  visible-whitespace <on|off|status> Configure visible whitespace characters "
                 "(default: disabled)\n";
    std::cout << "  hint <on|off|status>            Configure inline hints (default: enabled)\n";
    std::cout << "  multiline-indent <on|off|status> Configure auto-indent in multiline (default: "
                 "enabled)\n";
    std::cout << "  multiline <on|off|status>       Configure multiline input (default: enabled)\n";
    std::cout
        << "  inline-help <on|off|status>     Configure inline help messages (default: enabled)\n";
    std::cout << "  auto-tab <on|off|status>        Configure automatic tab completion (default: "
                 "enabled)\n";
    std::cout << "  keybind <subcommand> [...]       Inspect or modify key bindings (modifications "
                 "in config only)\n";
    std::cout << "    - Use 'keybind ext' for custom command keybindings\n";
    std::cout << "  generate-profile [--force]       Create or overwrite ~/.cjprofile\n";
    std::cout << "  generate-rc [--force]            Create or overwrite ~/.cjshrc\n";
    std::cout << "  generate-logout [--force]        Create or overwrite ~/.cjsh_logout\n";
    std::cout << "  set-max-bookmarks <number>       Limit stored directory bookmarks (10-1000)\n";
    std::cout << "  set-history-max <number|default|status> Configure history persistence\n";
    std::cout
        << "  bookmark-blacklist <subcommand>  Manage directories excluded from bookmarking\n";
    std::cout << "Use 'cjshopt <subcommand> --help' to see usage for a specific subcommand.\n";
}
}  // namespace

int cjshopt_command(const std::vector<std::string>& args) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) {
        if (!g_startup_active) {
            print_cjshopt_usage();
        }
        return 0;
    }

    if (args.size() < 2) {
        print_error(
            {ErrorType::INVALID_ARGUMENT,
             "cjshopt",
             "Missing subcommand argument",
             {
                 "Usage: cjshopt <subcommand> [options]",
                 "Available subcommands:",
                 "  style_def <token_type> <style>   Define or redefine a syntax "
                 "highlighting style",
                 "  login-startup-arg [--flag-name]  Add a startup flag "
                 "(config file only)",
                 "  completion-case <on|off|status>  Configure completion case "
                 "sensitivity (default: disabled)",
                 "  completion-spell <on|off|status> Configure completion spell correction "
                 "(default: enabled)",
                 "  line-numbers <on|off|relative|absolute|status>    Configure line numbers in "
                 "multiline input (default: enabled)",
                 "  current-line-number-highlight <on|off|status>    Configure current line "
                 "number highlighting (default: enabled)",
                 "  multiline-start-lines <count|status> Configure default multiline prompt "
                 "height (default: 1)",
                 "  hint-delay <milliseconds>        Set hint display delay in milliseconds",
                 "  completion-preview <on|off|status> Configure completion preview (default: "
                 "enabled)",
                 "  visible-whitespace <on|off|status> Configure visible whitespace characters "
                 "(default: disabled)",
                 "  hint <on|off|status>            Configure inline hints (default: enabled)",
                 "  multiline-indent <on|off|status> Configure auto-indent in multiline (default: "
                 "enabled)",
                 "  multiline <on|off|status>       Configure multiline input (default: enabled)",
                 "  inline-help <on|off|status>     Configure inline help messages (default: "
                 "enabled)",
                 "  auto-tab <on|off|status>        Configure automatic tab completion (default: "
                 "enabled)",
                 "  keybind <subcommand> [...]       Inspect or modify key bindings "
                 "(modifications in config only)",
                 "  generate-profile [--force]       Create or overwrite ~/.cjprofile",
                 "  generate-rc [--force]            Create or overwrite ~/.cjshrc",
                 "  generate-logout [--force]        Create or overwrite ~/.cjsh_logout",
                 "  set-max-bookmarks <number>       Limit stored directory bookmarks (10-1000)",
                 "  set-history-max <number|default|status> Configure history persistence",
                 "  bookmark-blacklist <subcommand>  Manage directories excluded from bookmarking",
             }});
        return 1;
    }

    const std::string& subcommand = args[1];

    if (subcommand == "style_def") {
        return style_def_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "login-startup-arg") {
        return startup_flag_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "completion-case") {
        return completion_case_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "completion-spell") {
        return completion_spell_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "line-numbers") {
        return line_numbers_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "current-line-number-highlight") {
        return current_line_number_highlight_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "multiline-start-lines") {
        return multiline_start_lines_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "hint-delay") {
        return hint_delay_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "completion-preview") {
        return completion_preview_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "visible-whitespace") {
        return visible_whitespace_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "hint") {
        return hint_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "multiline-indent") {
        return multiline_indent_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "multiline") {
        return multiline_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "inline-help") {
        return inline_help_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "auto-tab") {
        return auto_tab_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "keybind") {
        return keybind_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "generate-profile") {
        return generate_profile_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "generate-rc") {
        return generate_rc_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "generate-logout") {
        return generate_logout_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "set-max-bookmarks") {
        return set_max_bookmarks_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "set-history-max") {
        return set_history_max_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "bookmark-blacklist") {
        return bookmark_blacklist_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }

    print_error(
        {ErrorType::INVALID_ARGUMENT,
         "cjshopt",
         "unknown subcommand '" + subcommand + "'",
         {"Available subcommands: style_def, login-startup-arg, completion-case, completion-spell, "
          "line-numbers, current-line-number-highlight, multiline-start-lines, hint-delay, "
          "completion-preview, "
          "visible-whitespace, hint, multiline-indent, multiline, inline-help, auto-tab, keybind, "
          "generate-profile, "
          "generate-rc, generate-logout, set-max-bookmarks, set-history-max, bookmark-blacklist"}});
    return 1;
}
