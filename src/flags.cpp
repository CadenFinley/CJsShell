/*
  flags.cpp

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

#include "flags.h"

#include <getopt.h>
#include <unistd.h>

#include "error_out.h"
#include "isocline.h"
#include "shell_env.h"
#include "usage.h"

namespace flags {

std::vector<std::string>& startup_args() {
    static std::vector<std::string> args;
    return args;
}

std::vector<std::string>& profile_startup_args() {
    static std::vector<std::string> args;
    return args;
}

namespace {

constexpr int kOptNoCompletionLearning = 256;
constexpr int kOptNoSmartCd = 257;
std::vector<std::string> positional_parameters;

void detect_login_mode(char* argv[]) {
    // detect argv[0] being -cjsh
    if ((argv != nullptr) && (argv[0] != nullptr) && argv[0][0] == '-') {
        config::login_mode = true;
    }
}

void apply_minimal_mode() {
    // literally disable everything which turns cjsh into a worse bash or zsh or oh my zsh which is
    // pretty bad
    config::minimal_mode = true;
    config::colors_enabled = false;
    config::source_enabled = false;
    config::completions_enabled = false;
    config::completion_learning_enabled = false;
    config::smart_cd_enabled = false;
    config::syntax_highlighting_enabled = false;
    config::show_startup_time = false;
    config::show_title_line = false;
    config::history_expansion_enabled = false;
    ic_enable_line_numbers(false);
    ic_enable_multiline_indent(false);
}

}  // namespace

void save_startup_arguments(int argc, char* argv[]) {
    // save the startup args so that login-startup-arg in cjshopt can be used
    auto& args = startup_args();
    args.clear();
    for (int i = 0; i < argc; i++) {
        args.emplace_back(argv[i]);
    }
}

ParseResult parse_arguments(int argc, char* argv[]) {
    ParseResult result;

    detect_login_mode(argv);

    static struct option long_options[] = {
        {"login", no_argument, nullptr, 'l'},
        {"interactive", no_argument, nullptr, 'i'},
        {"command", required_argument, nullptr, 'c'},
        {"version", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {"no-colors", no_argument, nullptr, 'C'},
        {"no-titleline", no_argument, nullptr, 'L'},
        {"show-startup-time", no_argument, nullptr, 'U'},
        {"no-source", no_argument, nullptr, 'N'},
        {"no-completions", no_argument, nullptr, 'O'},
        {"no-completion-learning", no_argument, nullptr, kOptNoCompletionLearning},
        {"no-smart-cd", no_argument, nullptr, kOptNoSmartCd},
        {"no-syntax-highlighting", no_argument, nullptr, 'S'},
        {"startup-test", no_argument, nullptr, 'X'},
        {"minimal", no_argument, nullptr, 'm'},
        {"secure", no_argument, nullptr, 's'},
        {"no-history-expansion", no_argument, nullptr, 'H'},
        {"no-sh-warning", no_argument, nullptr, 'W'},
        {nullptr, 0, nullptr, 0}};

    const char* short_options = "+lic:vhCLUNOSXmsHW";

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
            case kOptNoCompletionLearning:
                config::completion_learning_enabled = false;
                break;
            case kOptNoSmartCd:
                config::smart_cd_enabled = false;
                break;
            case 'S':
                config::syntax_highlighting_enabled = false;
                break;
            case 'X':
                config::startup_test = true;
                break;
            case 'm':
                apply_minimal_mode();
                break;
            case 's':
                config::secure_mode = true;
                config::smart_cd_enabled = false;
                break;
            case 'H':
                config::history_expansion_enabled = false;
                break;
            case 'W':
                config::suppress_sh_warning = true;
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
        } else if (flag == "--no-completion-learning") {
            config::completion_learning_enabled = false;
        } else if (flag == "--no-smart-cd") {
            config::smart_cd_enabled = false;
        } else if (flag == "--no-syntax-highlighting") {
            config::syntax_highlighting_enabled = false;
        } else if (flag == "--startup-test") {
            config::startup_test = true;
        } else if (flag == "--interactive") {
            config::force_interactive = true;
        } else if (flag == "--login") {
        } else if (flag == "--minimal") {
            apply_minimal_mode();
        } else if (flag == "--secure") {
            config::secure_mode = true;
            config::smart_cd_enabled = false;
        } else if (flag == "--no-history-expansion") {
            config::history_expansion_enabled = false;
        } else if (flag == "--no-sh-warning") {
            config::suppress_sh_warning = true;
        }
    }
}

void set_positional_parameters(const std::vector<std::string>& params) {
    positional_parameters = params;
}

int shift_positional_parameters(int count) {
    if (count < 0) {
        return 1;
    }

    if (static_cast<size_t>(count) >= positional_parameters.size()) {
        positional_parameters.clear();
    } else {
        positional_parameters.erase(positional_parameters.begin(),
                                    positional_parameters.begin() + count);
    }

    return 0;
}

std::vector<std::string> get_positional_parameters() {
    return positional_parameters;
}

size_t get_positional_parameter_count() {
    return positional_parameters.size();
}

}  // namespace flags
