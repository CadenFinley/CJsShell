#include "flags.h"

#include <getopt.h>
#include <unistd.h>

#include "cjsh.h"
#include "error_out.h"
#include "isocline/isocline.h"
#include "usage.h"

std::vector<std::string>& startup_args() {
    static std::vector<std::string> args;
    return args;
}

std::vector<std::string>& profile_startup_args() {
    static std::vector<std::string> args;
    return args;
}

namespace flags {

namespace {

void detect_login_mode(char* argv[]) {
    if ((argv != nullptr) && (argv[0] != nullptr) && argv[0][0] == '-') {
        config::login_mode = true;
    }
}

void apply_minimal_mode() {
    config::minimal_mode = true;
    config::colors_enabled = false;
    config::source_enabled = false;
    config::completions_enabled = false;
    config::syntax_highlighting_enabled = false;
    config::smart_cd_enabled = false;
    config::show_startup_time = false;
    config::show_title_line = false;
    config::history_expansion_enabled = false;
    ic_enable_line_numbers(false);
    ic_enable_multiline_indent(false);
}

}  // namespace

void save_startup_arguments(int argc, char* argv[]) {
    auto& args = startup_args();
    args.clear();
    for (int i = 0; i < argc; i++) {
        args.emplace_back(argv[i]);
    }
}

ParseResult parse_arguments(int argc, char* argv[]) {
    ParseResult result;

    detect_login_mode(argv);

    static struct option long_options[] = {{"login", no_argument, nullptr, 'l'},
                                           {"interactive", no_argument, nullptr, 'i'},
                                           {"command", required_argument, nullptr, 'c'},
                                           {"version", no_argument, nullptr, 'v'},
                                           {"help", no_argument, nullptr, 'h'},
                                           {"no-colors", no_argument, nullptr, 'C'},
                                           {"no-titleline", no_argument, nullptr, 'L'},
                                           {"show-startup-time", no_argument, nullptr, 'U'},
                                           {"no-source", no_argument, nullptr, 'N'},
                                           {"no-completions", no_argument, nullptr, 'O'},
                                           {"no-syntax-highlighting", no_argument, nullptr, 'S'},
                                           {"no-smart-cd", no_argument, nullptr, 'M'},
                                           {"startup-test", no_argument, nullptr, 'X'},
                                           {"minimal", no_argument, nullptr, 'm'},
                                           {"secure", no_argument, nullptr, 's'},
                                           {"no-history-expansion", no_argument, nullptr, 'H'},
                                           {nullptr, 0, nullptr, 0}};

    const char* short_options = "+lic:vhCLUNOSMXmsH";

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
                config::history_expansion_enabled = false;
                break;
            case 'v':
                config::show_version = true;
                config::interactive_mode = false;
                break;
            case 'h':
                config::show_help = true;
                config::interactive_mode = false;
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
            case 's':
                config::secure_mode = true;
                break;
            case 'H':
                config::history_expansion_enabled = false;
                break;
            case '?':
                print_usage();
                result.exit_code = 127;
                result.should_exit = true;
                return result;
            default:
                print_error({ErrorType::INVALID_ARGUMENT,
                             std::string(1, static_cast<char>(c)),
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

    if (!config::force_interactive && (isatty(STDIN_FILENO) == 0)) {
        config::interactive_mode = false;
        config::history_expansion_enabled = false;
    }

    return result;
}

void apply_profile_startup_flags() {
    for (const std::string& flag : profile_startup_args()) {
        if (flag == "--no-colors") {
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
        } else if (flag == "--secure") {
            config::secure_mode = true;
        } else if (flag == "--no-history-expansion") {
            config::history_expansion_enabled = false;
        }
    }
}

}  // namespace flags
