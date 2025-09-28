#include "command_line_parser.h"

#include <getopt.h>
#include <unistd.h>

#include <iostream>

#include "cjsh.h"
#include "error_out.h"
#include "usage.h"

extern bool g_debug_mode;
extern bool g_title_line;

namespace cjsh {

CommandLineParser::ParseResult CommandLineParser::parse_arguments(int argc, char* argv[]) {
    ParseResult result;
    
    // Check if invoked as login shell (e.g., -cjsh)
    detect_login_mode(argv);

    static struct option long_options[] = {
        {"login", no_argument, 0, 'l'},
        {"interactive", no_argument, 0, 'i'},
        {"debug", no_argument, 0, 'd'},
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
        {0, 0, 0, 0}
    };
    
    const char* short_options = "+lic:vhdPTACLUNOSMXmD";  // Leading '+' enables POSIXLY_CORRECT behavior
    int option_index = 0;
    int c;
    optind = 1;

    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (c) {
            case 'l':
                config::login_mode = true;
                print_debug_info("Login mode enabled");
                break;
            case 'i':
                config::force_interactive = true;
                print_debug_info("Interactive mode forced");
                break;
            case 'd':
                g_debug_mode = true;
                std::cerr << "DEBUG: Debug mode enabled" << std::endl;
                break;
            case 'c':
                config::execute_command = true;
                config::cmd_to_execute = optarg;
                config::interactive_mode = false;
                print_debug_info("Command to execute: " + config::cmd_to_execute);
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
                print_debug_info("Plugins disabled");
                break;
            case 'T':
                config::themes_enabled = false;
                print_debug_info("Themes disabled");
                break;
            case 'A':
                config::ai_enabled = false;
                print_debug_info("AI disabled");
                break;
            case 'C':
                config::colors_enabled = false;
                print_debug_info("Colors disabled");
                break;
            case 'L':
                g_title_line = false;
                print_debug_info("Title line disabled");
                break;
            case 'U':
                config::show_startup_time = true;
                print_debug_info("Startup time display enabled");
                break;
            case 'N':
                config::source_enabled = false;
                print_debug_info("Source file disabled");
                break;
            case 'O':
                config::completions_enabled = false;
                print_debug_info("Completions disabled");
                break;
            case 'S':
                config::syntax_highlighting_enabled = false;
                print_debug_info("Syntax highlighting disabled");
                break;
            case 'M':
                config::smart_cd_enabled = false;
                print_debug_info("Smart cd disabled");
                break;
            case 'X':
                config::startup_test = true;
                print_debug_info("Startup test mode enabled");
                break;
            case 'm':
                apply_minimal_mode();
                print_debug_info("Minimal mode enabled - all features disabled");
                break;
            case 'D':
                config::disable_custom_ls = true;
                print_debug_info("Disable custom ls enabled");
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

    // Check if there are script files to execute
    if (optind < argc) {
        result.script_file = argv[optind];
        config::interactive_mode = false;
        print_debug_info("Script file specified: " + result.script_file);

        // Collect script arguments (everything after the script file name)
        for (int i = optind + 1; i < argc; i++) {
            result.script_args.push_back(argv[i]);
            print_debug_info("Script argument " + std::to_string(i - optind) + ": " + argv[i]);
        }
    }

    // Check if stdin is a terminal - if not, disable interactive mode
    if (!config::force_interactive && !isatty(STDIN_FILENO)) {
        config::interactive_mode = false;
        print_debug_info("Disabling interactive mode (stdin is not a terminal)");
    }

    return result;
}

void CommandLineParser::detect_login_mode(char* argv[]) {
    if (argv && argv[0] && argv[0][0] == '-') {
        config::login_mode = true;
        print_debug_info("Login mode detected from argv[0]: " + std::string(argv[0]));
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
    ::g_title_line = false;
}

void CommandLineParser::print_debug_info(const std::string& message) {
    if (g_debug_mode) {
        std::cerr << "DEBUG: " << message << std::endl;
    }
}

void CommandLineParser::apply_profile_startup_flags() {
    extern std::vector<std::string> g_profile_startup_args;
    extern bool g_title_line;
    
    // Apply startup flags that were collected during profile processing
    if (::g_debug_mode) {
        std::cerr << "DEBUG: Applying profile startup flags" << std::endl;
        if (::g_profile_startup_args.empty()) {
            std::cerr << "DEBUG: No profile startup flags to process"
                      << std::endl;
        } else {
            std::cerr << "DEBUG: Profile startup flags to process:"
                      << std::endl;
            for (const auto& flag : ::g_profile_startup_args) {
                std::cerr << "DEBUG:   " << flag << std::endl;
            }
        }
    }

    for (const std::string& flag : ::g_profile_startup_args) {
        if (::g_debug_mode)
            std::cerr << "DEBUG: Processing profile startup flag: " << flag
                      << std::endl;

        if (flag == "--debug") {
            ::g_debug_mode = true;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Debug mode enabled via profile"
                          << std::endl;
        } else if (flag == "--no-plugins") {
            config::plugins_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Plugins disabled via profile" << std::endl;
        } else if (flag == "--no-themes") {
            config::themes_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Themes disabled via profile" << std::endl;
        } else if (flag == "--no-ai") {
            config::ai_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: AI disabled via profile" << std::endl;
        } else if (flag == "--no-colors") {
            config::colors_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Colors disabled via profile" << std::endl;
        } else if (flag == "--no-titleline") {
            ::g_title_line = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Title line disabled via profile"
                          << std::endl;
        } else if (flag == "--show-startup-time") {
            config::show_startup_time = true;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Startup time display enabled via profile"
                          << std::endl;
        } else if (flag == "--no-source") {
            config::source_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Source file disabled via profile"
                          << std::endl;
        } else if (flag == "--no-completions") {
            config::completions_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Completions disabled via profile"
                          << std::endl;
        } else if (flag == "--no-syntax-highlighting") {
            config::syntax_highlighting_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Syntax highlighting disabled via profile"
                          << std::endl;
        } else if (flag == "--no-smart-cd") {
            config::smart_cd_enabled = false;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Smart cd disabled via profile"
                          << std::endl;
        } else if (flag == "--startup-test") {
            config::startup_test = true;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Startup test mode enabled via profile"
                          << std::endl;
        } else if (flag == "--interactive") {
            config::force_interactive = true;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Interactive mode forced via profile"
                          << std::endl;
        } else if (flag == "--login") {
            // Login mode is already set during initial argument processing
            if (::g_debug_mode)
                std::cerr << "DEBUG: Login mode flag found in profile (already "
                             "processed)"
                          << std::endl;
        } else if (flag == "--minimal") {
            apply_minimal_mode();
            if (::g_debug_mode)
                std::cerr << "DEBUG: Minimal mode enabled via profile - all "
                             "features disabled"
                          << std::endl;
        } else if (flag == "--disable-custom-ls") {
            config::disable_custom_ls = true;
            if (::g_debug_mode)
                std::cerr << "DEBUG: Disable custom ls enabled via profile"
                          << std::endl;
        }
    }
}

}  // namespace cjsh