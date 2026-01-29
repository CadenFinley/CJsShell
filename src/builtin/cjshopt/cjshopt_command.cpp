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
    std::cout << "  completion-learning <on|off|status> Toggle automatic completion learning "
                 "(default: enabled)\n";
    std::cout << "  line-numbers <on|off|relative|absolute|status>    Configure line numbers in "
                 "multiline input (default: enabled)\n";
    std::cout
        << "  line-numbers-replace-prompt <on|off|status>       Replace the final prompt line "
           "with line numbers (default: disabled)\n";
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
    std::cout << "  status-hints <off|normal|transient|persistent|status>  Control the default "
                 "status hint banner (default: normal)\n";
    std::cout << "  auto-tab <on|off|status>        Configure automatic tab completion (default: "
                 "enabled)\n";
    std::cout
        << "  prompt-newline <on|off|status>  Add a newline after command execution (default: "
           "disabled)\n";
    std::cout
        << "  prompt-cleanup <on|off|status>  Toggle prompt cleanup behavior (default: disabled)\n";
    std::cout
        << "  prompt-cleanup-newline <on|off|status>  Control cleanup newline behavior (default: "
           "disabled)\n";
    std::cout
        << "  prompt-cleanup-empty-line <on|off|status>  Control cleanup empty line insertion "
           "(default: disabled)\n";
    std::cout << "  prompt-cleanup-truncate <on|off|status>  Control cleanup multiline truncation "
                 "(default: disabled)\n";
    std::cout << "  right-prompt-follow-cursor <on|off|status>  Re-anchor the inline right prompt "
                 "to the cursor row (default: disabled)\n";
    std::cout << "  keybind <subcommand> [...]       Inspect or modify key bindings (modifications "
                 "in config only)\n";
    std::cout << "    - Use 'keybind ext' for custom command keybindings\n";
    std::cout << "  history-single-io <on|off|status> Toggle single read/write history mode "
                 "(default: enabled)\n";
    std::cout << "  generate-profile [--force] [--alt]       Create or overwrite ~/.cjprofile\n";
    std::cout << "  generate-rc [--force] [--alt]            Create or overwrite ~/.cjshrc\n";
    std::cout << "  generate-logout [--force] [--alt]        Create or overwrite ~/.cjsh_logout\n";
    std::cout << "  set-history-max <number|default|status> Configure history persistence\n";
    std::cout << "  set-completion-max <number|default|status> Limit completion suggestions\n";
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
                 std::string("  style_def <token_type> <style>   Define or redefine a syntax ") +
                     "highlighting style",
                 "  login-startup-arg [--flag-name]  Add a startup flag (config file only)",
                 std::string("  completion-case <on|off|status>  Configure completion case ") +
                     "sensitivity (default: disabled)",
                 std::string("  completion-spell <on|off|status> Configure completion spell ") +
                     "correction (default: enabled)",
                 std::string("  completion-learning <on|off|status> Toggle automatic completion ") +
                     "learning (default: enabled)",
                 std::string(
                     "  history-single-io <on|off|status> Toggle single read/write history ") +
                     "mode (default: enabled)",
                 std::string(
                     "  line-numbers <on|off|relative|absolute|status>    Configure line ") +
                     "numbers in multiline input (default: enabled)",
                 std::string(
                     "  line-numbers-replace-prompt <on|off|status>       Replace the final ") +
                     "prompt line with line numbers (default: disabled)",
                 std::string(
                     "  current-line-number-highlight <on|off|status>    Configure current ") +
                     "line number highlighting (default: enabled)",
                 std::string(
                     "  multiline-start-lines <count|status> Configure default multiline ") +
                     "prompt height (default: 1)",
                 "  hint-delay <milliseconds>        Set hint display delay in milliseconds",
                 std::string("  completion-preview <on|off|status> Configure completion preview ") +
                     "(default: enabled)",
                 std::string("  visible-whitespace <on|off|status> Configure visible whitespace ") +
                     "characters (default: disabled)",
                 "  hint <on|off|status>            Configure inline hints (default: enabled)",
                 std::string(
                     "  multiline-indent <on|off|status> Configure auto-indent in multiline ") +
                     "(default: enabled)",
                 "  multiline <on|off|status>       Configure multiline input (default: enabled)",
                 std::string("  inline-help <on|off|status>     Configure inline help messages ") +
                     "(default: enabled)",
                 std::string(
                     "  status-hints <off|normal|transient|persistent|status>  Control the ") +
                     "default status hint banner (default: normal)",
                 std::string(
                     "  auto-tab <on|off|status>        Configure automatic tab completion ") +
                     "(default: enabled)",
                 std::string(
                     "  prompt-newline <on|off|status>  Add a newline after command execution ") +
                     "(default: disabled)",
                 std::string("  prompt-cleanup <on|off|status>  Toggle prompt cleanup behavior ") +
                     "(default: disabled)",
                 std::string("  prompt-cleanup-newline <on|off|status>  Control cleanup newline "
                             "behavior ") +
                     "(default: disabled)",
                 std::string("  prompt-cleanup-empty-line <on|off|status>  Control cleanup empty "
                             "line insertion ") +
                     "(default: disabled)",
                 std::string("  prompt-cleanup-truncate <on|off|status>  Control cleanup multiline "
                             "truncation ") +
                     "(default: disabled)",
                 std::string("  right-prompt-follow-cursor <on|off|status>  Move the inline right "
                             "prompt with the cursor ") +
                     "(default: disabled)",
                 std::string("  keybind <subcommand> [...]       Inspect or modify key bindings ") +
                     "(modifications in config only)",
                 "  line-numbers-continuation <on|off|status> Control line numbers when a "
                 "continuation prompt is active",
                 "  generate-profile [--force] [--alt]       Create or overwrite ~/.cjprofile",
                 "  generate-rc [--force] [--alt]            Create or overwrite ~/.cjshrc",
                 "  generate-logout [--force] [--alt]        Create or overwrite ~/.cjsh_logout",
                 "  set-history-max <number|default|status> Configure history persistence",
                 "  set-completion-max <number|default|status> Limit completion suggestions",

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
    if (subcommand == "completion-learning") {
        return completion_learning_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "history-single-io") {
        return history_single_io_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "line-numbers") {
        return line_numbers_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "line-numbers-continuation") {
        return line_numbers_continuation_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "line-numbers-replace-prompt") {
        return line_numbers_replace_prompt_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
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
    if (subcommand == "status-hints") {
        return status_hints_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "auto-tab") {
        return auto_tab_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "prompt-newline") {
        return prompt_newline_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "prompt-cleanup") {
        return prompt_cleanup_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "prompt-cleanup-newline") {
        return prompt_cleanup_newline_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "prompt-cleanup-empty-line") {
        return prompt_cleanup_empty_line_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "prompt-cleanup-truncate") {
        return prompt_cleanup_truncate_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "right-prompt-follow-cursor") {
        return right_prompt_follow_cursor_command(
            std::vector<std::string>(args.begin() + 1, args.end()));
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
    if (subcommand == "set-history-max") {
        return set_history_max_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    if (subcommand == "set-completion-max") {
        return set_completion_max_command(std::vector<std::string>(args.begin() + 1, args.end()));
    }
    print_error(
        {ErrorType::INVALID_ARGUMENT,
         "cjshopt",
         "unknown subcommand '" + subcommand + "'",
         {"Available subcommands: style_def, login-startup-arg, completion-case, completion-spell, "
          "completion-learning, "
          "line-numbers, line-numbers-continuation, line-numbers-replace-prompt, "
          "current-line-number-highlight, multiline-start-lines, hint-delay, "

          "completion-preview, visible-whitespace, hint, multiline-indent, multiline, inline-help, "
          "status-hints, auto-tab, prompt-newline, prompt-cleanup, prompt-cleanup-newline, "
          "prompt-cleanup-empty-line, prompt-cleanup-truncate, right-prompt-follow-cursor, "
          "keybind, "
          "generate-rc, generate-logout, set-history-max, set-completion-max"}});

    return 1;
}
