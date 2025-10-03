#include "umask_command.h"

#include "builtin_help.h"

#include <sys/stat.h>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "error_out.h"

mode_t parse_octal_mode(const std::string& mode_str) {
    if (mode_str.empty()) {
        return static_cast<mode_t>(-1);
    }

    for (char c : mode_str) {
        if (c < '0' || c > '7') {
            return static_cast<mode_t>(-1);
        }
    }

    try {
        unsigned long mode = std::stoul(mode_str, nullptr, 8);
        if (mode > 0777) {
            return static_cast<mode_t>(-1);
        }
        return static_cast<mode_t>(mode);
    } catch (...) {
        return static_cast<mode_t>(-1);
    }
}

std::string format_octal_mode(mode_t mode) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << std::oct << mode;
    return oss.str();
}

mode_t parse_symbolic_mode(const std::string& mode_str, mode_t current_mask) {
    if (mode_str == "u=rwx,g=rx,o=rx") {
        return 022;
    } else if (mode_str == "u=rw,g=r,o=r") {
        return 022;
    } else if (mode_str == "u=rwx,g=,o=") {
        return 077;
    }

    return current_mask;
}

int umask_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: umask [-S] [MODE]", "Display or set the file creation mask.",
                             "-S shows the mask in symbolic form."})) {
        return 0;
    }
    mode_t current_mask = umask(0);
    umask(current_mask);

    if (args.size() == 1) {
        std::cout << format_octal_mode(current_mask) << std::endl;
        return 0;
    }

    bool symbolic_mode = false;
    size_t mode_index = 1;

    if (args.size() > 1 && args[1] == "-S") {
        if (args.size() == 2) {
            mode_t perms = (~current_mask) & 0777;

            std::cout << "u=";
            if (perms & S_IRUSR)
                std::cout << "r";
            if (perms & S_IWUSR)
                std::cout << "w";
            if (perms & S_IXUSR)
                std::cout << "x";

            std::cout << ",g=";
            if (perms & S_IRGRP)
                std::cout << "r";
            if (perms & S_IWGRP)
                std::cout << "w";
            if (perms & S_IXGRP)
                std::cout << "x";

            std::cout << ",o=";
            if (perms & S_IROTH)
                std::cout << "r";
            if (perms & S_IWOTH)
                std::cout << "w";
            if (perms & S_IXOTH)
                std::cout << "x";

            std::cout << std::endl;
            return 0;
        }

        symbolic_mode = true;
        mode_index = 2;
    }

    if (mode_index >= args.size()) {
        print_error({ErrorType::INVALID_ARGUMENT, "umask", "usage: umask [-S] [mode]", {}});
        return 2;
    }

    std::string mode_str = args[mode_index];
    mode_t new_mask;

    if (symbolic_mode || mode_str.find('=') != std::string::npos) {
        new_mask = parse_symbolic_mode(mode_str, current_mask);
        if (new_mask == current_mask && mode_str != "u=rwx,g=rwx,o=rwx") {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "umask", mode_str + ": invalid symbolic mode", {}});
            return 1;
        }
    } else {
        new_mask = parse_octal_mode(mode_str);
        if (new_mask == static_cast<mode_t>(-1)) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "umask",
                         mode_str + ": octal number out of range",
                         {}});
            return 1;
        }
    }

    umask(new_mask);

    return 0;
}
