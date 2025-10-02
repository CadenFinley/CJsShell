#include "command_line_parser.h"

#include <getopt.h>
#include <unistd.h>

#include <iostream>

#include "cjsh.h"
#include "error_out.h"
#include "usage.h"

namespace cjsh {

CommandLineParser::ParseResult CommandLineParser::parse_arguments(int argc, char* argv[]) {
    ParseResult result;

    detect_login_mode(argv);

    static struct option long_options[] = {{"login", no_argument, 0, 'l'},
                                           {"interactive", no_argument, 0, 'i'},
                                           {"command", required_argument, 0, 'c'},
                                           {"version", no_argument, 0, 'v'},
                                           {"help", no_argument, 0, 'h'},
                                           {"no-plugins", no_argument, 0, 'P'},
                                           {"no-themes", no_argument, 0, 'T'},
                                           {"no-ai", no_argument, 0, 'A'},
                                           {"no-colors", no_argument, 0, 'C'},
                                           {"no-titleline", no_argument, 0, 'L'},
                                           {"show-startup-time", no_argument, 0, 'U'},
                                           {"no-source", no_argument, 0, 'N'},
                                           {"no-completions", no_argument, 0, 'O'},
                                           {"no-syntax-highlighting", no_argument, 0, 'S'},
                                           {"no-smart-cd", no_argument, 0, 'M'},
                                           {"startup-test", no_argument, 0, 'X'},
                                           {"minimal", no_argument, 0, 'm'},
                                           {"disable-custom-ls", no_argument, 0, 'D'},
                                           {"secure", no_argument, 0, 's'},
                                           {0, 0, 0, 0}};

    const char* short_options = "+lic:vhPTACLUNOSMXmDs";

    int option_index = 0;
    int c;
    optind = 1;

    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (c) {
            case 'l':
                config::login_mode = true;
                break;
            case 'i':
                config::force_interactive = true;
                break;
            case 'c':
                config::execute_command = true;
                config::cmd_to_execute = optarg;
                config::interactive_mode = false;
                break;
            case 'v':
                config::show_version = true;
                config::interactive_mode = false;
                break;
            case 'h':
                config::show_help = true;
                config::interactive_mode = false;
                break;
            case 'P':
                config::plugins_enabled = false;
                break;
            case 'T':
                config::themes_enabled = false;
                break;
            case 'A':
                config::ai_enabled = false;
                break;
            case 'C':
                config::colors_enabled = false;
                break;
            case 'L':
                config::show_title_line = false;
                break;
            case 'U':
                config::show_startup_time = true;
                break;
            case 'N':
                config::source_enabled = false;
                break;
            case 'O':
                config::completions_enabled = false;
                break;
            case 'S':
                config::syntax_highlighting_enabled = false;
                break;
            case 'M':
                config::smart_cd_enabled = false;
                break;
            case 'X':
                config::startup_test = true;
                break;
            case 'm':
                apply_minimal_mode();
                break;
            case 'D':
                config::disable_custom_ls = true;
                break;
            case 's':
                config::secure_mode = true;
                break;
            case '?':
                print_usage();
                result.exit_code = 127;
                result.should_exit = true;
                return result;
            default:
                print_error({ErrorType::INVALID_ARGUMENT,
                             std::string(1, c),
                             "Unrecognized option",
                             {"Check command line arguments"}});
                result.exit_code = 127;
                result.should_exit = true;
                return result;
        }
    }

    if (optind < argc) {
        result.script_file = argv[optind];
        config::interactive_mode = false;

        for (int i = optind + 1; i < argc; i++) {
            result.script_args.push_back(argv[i]);
        }
    }

    if (!config::force_interactive && !isatty(STDIN_FILENO)) {
        config::interactive_mode = false;
    }

    return result;
}

void CommandLineParser::detect_login_mode(char* argv[]) {
    if (argv && argv[0] && argv[0][0] == '-') {
        config::login_mode = true;
    }
}

void CommandLineParser::apply_minimal_mode() {
    config::minimal_mode = true;
    config::plugins_enabled = false;
    config::themes_enabled = false;
    config::ai_enabled = false;
    config::colors_enabled = false;
    config::source_enabled = false;
    config::completions_enabled = false;
    config::syntax_highlighting_enabled = false;
    config::smart_cd_enabled = false;
    config::disable_custom_ls = true;
    config::show_startup_time = false;
    config::show_title_line = false;
}

void CommandLineParser::apply_profile_startup_flags() {
    extern std::vector<std::string> g_profile_startup_args;

    for (const std::string& flag : ::g_profile_startup_args) {
        if (flag == "--no-plugins") {
            config::plugins_enabled = false;
        } else if (flag == "--no-themes") {
            config::themes_enabled = false;
        } else if (flag == "--no-ai") {
            config::ai_enabled = false;
        } else if (flag == "--no-colors") {
            config::colors_enabled = false;
        } else if (flag == "--no-titleline") {
            config::show_title_line = false;
        } else if (flag == "--show-startup-time") {
            config::show_startup_time = true;
        } else if (flag == "--no-source") {
            config::source_enabled = false;
        } else if (flag == "--no-completions") {
            config::completions_enabled = false;
        } else if (flag == "--no-syntax-highlighting") {
            config::syntax_highlighting_enabled = false;
        } else if (flag == "--no-smart-cd") {
            config::smart_cd_enabled = false;
        } else if (flag == "--startup-test") {
            config::startup_test = true;
        } else if (flag == "--interactive") {
            config::force_interactive = true;
        } else if (flag == "--login") {
        } else if (flag == "--minimal") {
            apply_minimal_mode();
        } else if (flag == "--disable-custom-ls") {
            config::disable_custom_ls = true;
        } else if (flag == "--secure") {
            config::secure_mode = true;
        }
    }
}

}  // namespace cjsh