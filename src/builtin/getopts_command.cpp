#include "getopts_command.h"

#include "builtin_help.h"

#include <cstdlib>
#include "cjsh.h"
#include "error_out.h"
#include "shell.h"
#include "shell_script_interpreter.h"

static void set_special_var(Shell* shell, const std::string& key, const std::string& value) {
    setenv(key.c_str(), value.c_str(), 1);
    if (shell) {
        auto* interpreter = shell->get_shell_script_interpreter();
        if (interpreter && interpreter->is_local_variable(key)) {
            interpreter->set_local_variable(key, value);
        } else {
            shell->get_env_vars()[key] = value;
        }
    }
}

static void set_special_var(Shell* shell, const std::string& key, int value) {
    set_special_var(shell, key, std::to_string(value));
}

static void unset_special_var(Shell* shell, const std::string& key) {
    unsetenv(key.c_str());
    if (shell) {
        auto* interpreter = shell->get_shell_script_interpreter();
        if (interpreter && interpreter->is_local_variable(key)) {
            interpreter->unset_local_variable(key);
        } else {
            shell->get_env_vars().erase(key);
        }
    }
}

static void set_getopts_pos(int value) {
    std::string buffer = std::to_string(value);
    setenv("GETOPTS_POS", buffer.c_str(), 1);
}

int getopts_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args,
                            {"Usage: getopts OPTSTRING NAME [ARG ...]",
                             "Parse positional parameters as options, storing results in NAME.",
                             "With no ARG values, uses the shell's positional parameters."})) {
        return 0;
    }
    if (!shell) {
        print_error({ErrorType::RUNTIME_ERROR, "getopts", "shell not available", {}});
        return 1;
    }

    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "getopts",
                     "usage: getopts optstring name [args...]",
                     {}});
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
    std::string optind_source;
    if (auto* interpreter = shell->get_shell_script_interpreter()) {
        optind_source = interpreter->get_variable_value("OPTIND");
    }
    if (optind_source.empty()) {
        if (const char* optind_env = getenv("OPTIND")) {
            optind_source = optind_env;
        }
    }
    if (!optind_source.empty()) {
        try {
            optind = std::stoi(optind_source);
        } catch (...) {
            optind = 1;
        }
    }

    if (optind <= 0) {
        optind = 1;
    }

    if (argv_list.empty() || optind > static_cast<int>(argv_list.size())) {
        set_special_var(shell, "OPTIND", optind);
        setenv("GETOPTS_POS", "1", 1);
        return 1;
    }

    std::string current_arg = argv_list[optind - 1];

    if (current_arg.size() < 2 || current_arg[0] != '-') {
        set_special_var(shell, "OPTIND", optind);
        setenv("GETOPTS_POS", "1", 1);
        return 1;
    }

    if (current_arg == "--") {
        set_special_var(shell, "OPTIND", optind + 1);
        setenv("GETOPTS_POS", "1", 1);
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
    } else {
        char_index = 1;
    }

    if (char_index >= static_cast<int>(current_arg.size())) {
        optind++;
        char_index = 1;
        set_special_var(shell, "OPTIND", optind);
        setenv("GETOPTS_POS", "1", 1);
        return getopts_command(args, shell);
    }

    char opt = current_arg[char_index];
    char_index++;

    size_t opt_pos = optstring.find(opt);
    if (opt_pos == std::string::npos) {
        set_special_var(shell, name, "?");

        std::string optarg_val(1, opt);
        set_special_var(shell, "OPTARG", optarg_val);

        if (char_index >= static_cast<int>(current_arg.size())) {
            optind++;
            char_index = 1;
        }
        set_special_var(shell, "OPTIND", optind);
        set_getopts_pos(char_index);

        if (!optstring.empty() && optstring[0] == ':') {
            set_special_var(shell, name, "?");
            return 0;
        }

        print_error(
            {ErrorType::INVALID_ARGUMENT, "getopts", std::string("illegal option -- ") + opt, {}});
        return 0;
    }

    std::string opt_val(1, opt);
    set_special_var(shell, name, opt_val);

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
                    set_special_var(shell, name, ":");
                    set_special_var(shell, "OPTARG", opt_val);
                } else {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "getopts",
                                 std::string("option requires an argument -- ") + opt,
                                 {}});
                    set_special_var(shell, name, "?");
                    set_special_var(shell, "OPTARG", opt_val);
                }
                set_special_var(shell, "OPTIND", optind);
                setenv("GETOPTS_POS", "1", 1);
                return 0;
            }
            char_index = 1;
        }

        set_special_var(shell, "OPTARG", optarg);
    } else {
        unset_special_var(shell, "OPTARG");

        if (char_index >= static_cast<int>(current_arg.size())) {
            optind++;
            char_index = 1;
        }
    }

    set_special_var(shell, "OPTIND", optind);
    set_getopts_pos(char_index);

    return 0;
}
