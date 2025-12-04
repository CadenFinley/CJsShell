#include "set_command.h"

#include "builtin_help.h"

#include <iostream>
#include <string>
#include <vector>

#include "cjsh.h"
#include "error_out.h"
#include "shell.h"

int set_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args, {"Usage: set [-+eCunxvfna] [-o option] [--] [ARG ...]",
                                   "Set or unset shell options and positional parameters.",
                                   "",
                                   "Options:",
                                   "  -e              Exit on error (errexit)",
                                   "  -C              Prevent file overwriting (noclobber)",
                                   "  -u              Treat unset variables as error (nounset)",
                                   "  -x              Print commands before execution (xtrace)",
                                   "  -v              Print input lines as they are read (verbose)",
                                   "  -n              Read but don't execute commands (noexec)",
                                   "  -f              Disable pathname expansion (noglob)",
                                   "  -a              Auto-export modified variables (allexport)",
                                   "  -o option       Set option by name (huponexit, etc.)",
                                   "  +<option>       Unset the specified option",
                                   "  --              End options; remaining args set $1, $2, etc.",
                                   "",
                                   "With no arguments, print all environment variables.",
                                   "Use 'set -o' to list current option settings.",
                                   "",
                                   "Special options:",
                                   "  --errexit-severity=LEVEL  Set errexit sensitivity level"})) {
        return 0;
    }
    if (shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "set", "shell not available", {}});
        return 1;
    }

    if (args.size() == 1) {
        extern char** environ;
        for (char** env = environ; *env != nullptr; ++env) {
            std::cout << *env << '\n';
        }
        return 0;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "-e" || (arg == "-o" && i + 1 < args.size() && args[i + 1] == "errexit")) {
            shell->set_shell_option("errexit", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+e" ||
                   (arg == "+o" && i + 1 < args.size() && args[i + 1] == "errexit")) {
            shell->set_shell_option("errexit", false);
            if (arg == "+o") {
                ++i;
            }
        } else if (arg == "-C" ||
                   (arg == "-o" && i + 1 < args.size() && args[i + 1] == "noclobber")) {
            shell->set_shell_option("noclobber", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+C" ||
                   (arg == "+o" && i + 1 < args.size() && args[i + 1] == "noclobber")) {
            shell->set_shell_option("noclobber", false);
            if (arg == "+o") {
                ++i;
            }
        } else if (arg == "-u" ||
                   (arg == "-o" && i + 1 < args.size() && args[i + 1] == "nounset")) {
            shell->set_shell_option("nounset", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+u" ||
                   (arg == "+o" && i + 1 < args.size() && args[i + 1] == "nounset")) {
            shell->set_shell_option("nounset", false);
            if (arg == "+o") {
                ++i;
            }
        } else if (arg == "-x" || (arg == "-o" && i + 1 < args.size() && args[i + 1] == "xtrace")) {
            shell->set_shell_option("xtrace", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+x" || (arg == "+o" && i + 1 < args.size() && args[i + 1] == "xtrace")) {
            shell->set_shell_option("xtrace", false);
            if (arg == "+o") {
                ++i;
            }
        } else if (arg == "-v" ||
                   (arg == "-o" && i + 1 < args.size() && args[i + 1] == "verbose")) {
            shell->set_shell_option("verbose", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+v" ||
                   (arg == "+o" && i + 1 < args.size() && args[i + 1] == "verbose")) {
            shell->set_shell_option("verbose", false);
            if (arg == "+o") {
                ++i;
            }
        } else if (arg == "-n" || (arg == "-o" && i + 1 < args.size() && args[i + 1] == "noexec")) {
            shell->set_shell_option("noexec", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+n" || (arg == "+o" && i + 1 < args.size() && args[i + 1] == "noexec")) {
            shell->set_shell_option("noexec", false);
            if (arg == "+o") {
                ++i;
            }
        } else if (arg == "-f" || (arg == "-o" && i + 1 < args.size() && args[i + 1] == "noglob")) {
            shell->set_shell_option("noglob", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+f" || (arg == "+o" && i + 1 < args.size() && args[i + 1] == "noglob")) {
            shell->set_shell_option("noglob", false);
            if (arg == "+o") {
                ++i;
            }
        } else if (arg == "-a" ||
                   (arg == "-o" && i + 1 < args.size() && args[i + 1] == "allexport")) {
            shell->set_shell_option("allexport", true);
            if (arg == "-o") {
                ++i;
            }
        } else if (arg == "+a" ||
                   (arg == "+o" && i + 1 < args.size() && args[i + 1] == "allexport")) {
            shell->set_shell_option("allexport", false);
            if (arg == "+o") {
                ++i;
            }
        } else if ((arg == "-o" && i + 1 < args.size() && args[i + 1] == "huponexit")) {
            shell->set_shell_option("huponexit", true);
            ++i;
        } else if ((arg == "+o" && i + 1 < args.size() && args[i + 1] == "huponexit")) {
            shell->set_shell_option("huponexit", false);
            ++i;
        } else if (arg == "-o" && i + 1 < args.size() &&
                   args[i + 1].find("errexit_severity=") == 0) {
            std::string severity = args[i + 1].substr(17);
            shell->set_errexit_severity(severity);
            ++i;
        } else if (arg.find("--errexit-severity=") == 0) {
            std::string severity = arg.substr(19);
            shell->set_errexit_severity(severity);
        } else if (arg == "-o" && i + 1 >= args.size()) {
            std::cout << "errexit        \t" << (shell->get_shell_option("errexit") ? "on" : "off")
                      << '\n';
            std::cout << "noclobber      \t"
                      << (shell->get_shell_option("noclobber") ? "on" : "off") << '\n';
            std::cout << "nounset        \t" << (shell->get_shell_option("nounset") ? "on" : "off")
                      << '\n';
            std::cout << "xtrace         \t" << (shell->get_shell_option("xtrace") ? "on" : "off")
                      << '\n';
            std::cout << "verbose        \t" << (shell->get_shell_option("verbose") ? "on" : "off")
                      << '\n';
            std::cout << "noexec         \t" << (shell->get_shell_option("noexec") ? "on" : "off")
                      << '\n';
            std::cout << "noglob         \t" << (shell->get_shell_option("noglob") ? "on" : "off")
                      << '\n';
            std::cout << "allexport      \t"
                      << (shell->get_shell_option("allexport") ? "on" : "off") << '\n';
            std::cout << "huponexit      \t"
                      << (shell->get_shell_option("huponexit") ? "on" : "off") << '\n';
            std::cout << "errexit_severity\t" << shell->get_errexit_severity() << '\n';
            return 0;
        } else if (arg.substr(0, 2) == "--") {
            std::vector<std::string> positional_params;
            for (size_t j = i + 1; j < args.size(); ++j) {
                positional_params.push_back(args[j]);
            }

            shell->set_positional_parameters(positional_params);
            return 0;
        } else {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "set", "option '" + arg + "' not supported yet", {}});
            return 1;
        }
    }

    return 0;
}

int shift_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(
            args, {"Usage: shift [N]", "Discard the first N positional parameters (default 1)."})) {
        return 0;
    }
    if (shell == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR, "shift", "shell not available", {}});
        return 1;
    }

    int shift_count = 1;

    if (args.size() > 1) {
        try {
            shift_count = std::stoi(args[1]);
            if (shift_count < 0) {
                print_error({ErrorType::INVALID_ARGUMENT, "shift", "negative shift count", {}});
                return 1;
            }
        } catch (const std::exception&) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "shift", "invalid shift count: " + args[1], {}});
            return 1;
        }
    }

    return shell->shift_positional_parameters(shift_count);
}
