#include "getopts_command.h"
#include <cstdlib>
#include "cjsh.h"
#include "error_out.h"
#include "shell.h"

int getopts_command(const std::vector<std::string>& args, Shell* shell) {
    if (!shell) {
        print_error({ErrorType::RUNTIME_ERROR, "getopts", "shell not available", {}});
        return 1;
    }

    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "getopts", "usage: getopts optstring name [args...]", {}});
        return 1;
    }

    std::string optstring = args[1];
    std::string name = args[2];

    std::vector<std::string> argv_list;
    if (args.size() > 3) {
        for (size_t i = 3; i < args.size(); ++i) {
            argv_list.push_back(args[i]);
        }
    } else {
        argv_list = shell->get_positional_parameters();
    }

    int optind = 1;
    const char* optind_env = getenv("OPTIND");
    if (optind_env) {
        try {
            optind = std::stoi(optind_env);
        } catch (...) {
            optind = 1;
        }
    }

    if (optind > static_cast<int>(argv_list.size())) {
        setenv("OPTIND", "1", 1);
        return 1;
    }

    if (optind <= 0 || optind > static_cast<int>(argv_list.size())) {
        setenv("OPTIND", "1", 1);
        return 1;
    }

    std::string current_arg = argv_list[optind - 1];

    if (current_arg.size() < 2 || current_arg[0] != '-') {
        setenv("OPTIND", "1", 1);
        return 1;
    }

    if (current_arg == "--") {
        setenv("OPTIND", std::to_string(optind + 1).c_str(), 1);
        return 1;
    }

    static int char_index = 1;
    const char* optarg_env = getenv("GETOPTS_POS");
    if (optarg_env) {
        try {
            char_index = std::stoi(optarg_env);
        } catch (...) {
            char_index = 1;
        }
    }

    if (char_index == 1) {
        char_index = 1;
    }

    if (char_index >= static_cast<int>(current_arg.size())) {
        optind++;
        char_index = 1;
        setenv("OPTIND", std::to_string(optind).c_str(), 1);
        setenv("GETOPTS_POS", "1", 1);

        return getopts_command(args, shell);
    }

    char opt = current_arg[char_index];
    char_index++;

    size_t opt_pos = optstring.find(opt);
    if (opt_pos == std::string::npos) {
        setenv(name.c_str(), "?", 1);

        std::string optarg_val(1, opt);
        setenv("OPTARG", optarg_val.c_str(), 1);

        if (char_index >= static_cast<int>(current_arg.size())) {
            optind++;
            char_index = 1;
        }
        setenv("OPTIND", std::to_string(optind).c_str(), 1);
        setenv("GETOPTS_POS", std::to_string(char_index).c_str(), 1);

        if (!optstring.empty() && optstring[0] == ':') {
            setenv(name.c_str(), "?", 1);
            return 0;
        }

        print_error({ErrorType::INVALID_ARGUMENT, "getopts", std::string("illegal option -- ") + opt, {}});
        return 0;
    }

    std::string opt_val(1, opt);
    setenv(name.c_str(), opt_val.c_str(), 1);

    if (opt_pos + 1 < optstring.size() && optstring[opt_pos + 1] == ':') {
        std::string optarg;

        if (char_index < static_cast<int>(current_arg.size())) {
            optarg = current_arg.substr(char_index);
            optind++;
            char_index = 1;
        } else {
            optind++;
            if (optind <= static_cast<int>(argv_list.size())) {
                optarg = argv_list[optind - 1];
                optind++;
            } else {
                if (!optstring.empty() && optstring[0] == ':') {
                    setenv(name.c_str(), ":", 1);
                    setenv("OPTARG", opt_val.c_str(), 1);
                } else {
                    print_error({ErrorType::INVALID_ARGUMENT, "getopts", std::string("option requires an argument -- ") + opt, {}});
                    setenv(name.c_str(), "?", 1);
                    setenv("OPTARG", opt_val.c_str(), 1);
                }
                setenv("OPTIND", std::to_string(optind).c_str(), 1);
                setenv("GETOPTS_POS", "1", 1);
                return 0;
            }
            char_index = 1;
        }

        setenv("OPTARG", optarg.c_str(), 1);
    } else {
        unsetenv("OPTARG");

        if (char_index >= static_cast<int>(current_arg.size())) {
            optind++;
            char_index = 1;
        }
    }

    setenv("OPTIND", std::to_string(optind).c_str(), 1);
    setenv("GETOPTS_POS", std::to_string(char_index).c_str(), 1);

    return 0;
}
