#include "test_command.h"

#include "builtin_help.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

int test_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: test EXPRESSION", "Evaluate file attributes and comparisons.",
                             "Aliases: [ EXPRESSION ]"})) {
        return 0;
    }
    if (args.empty()) {
        return 1;
    }

    std::vector<std::string> test_args = args;
    if (args[0] == "[" && args.size() > 1 && args.back() == "]") {
        test_args.pop_back();
        test_args.erase(test_args.begin());
    } else if (args[0] == "test") {
        test_args.erase(test_args.begin());
    } else if (args[0] == "[") {
        test_args.erase(test_args.begin());
    }

    if (test_args.empty()) {
        return 1;
    }

    if (test_args.size() == 1) {
        return test_args[0].empty() ? 1 : 0;
    }

    if (test_args.size() == 2) {
        const std::string& op = test_args[0];
        const std::string& arg = test_args[1];

        if (op == "-z") {
            return arg.empty() ? 0 : 1;
        } else if (op == "-n") {
            return arg.empty() ? 1 : 0;
        } else if (op == "-f") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
        } else if (op == "-d") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
        } else if (op == "-e") {
            return access(arg.c_str(), F_OK) == 0 ? 0 : 1;
        } else if (op == "-r") {
            return access(arg.c_str(), R_OK) == 0 ? 0 : 1;
        } else if (op == "-w") {
            return access(arg.c_str(), W_OK) == 0 ? 0 : 1;
        } else if (op == "-x") {
            return access(arg.c_str(), X_OK) == 0 ? 0 : 1;
        } else if (op == "-s") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && st.st_size > 0) ? 0 : 1;
        } else if (op == "-L") {
            struct stat st;
            return (lstat(arg.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) ? 0 : 1;
        } else if (op == "-p") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISFIFO(st.st_mode)) ? 0 : 1;
        } else if (op == "-b") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISBLK(st.st_mode)) ? 0 : 1;
        } else if (op == "-c") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISCHR(st.st_mode)) ? 0 : 1;
        } else if (op == "-S") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && S_ISSOCK(st.st_mode)) ? 0 : 1;
        } else if (op == "-u") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && (st.st_mode & S_ISUID)) ? 0 : 1;
        } else if (op == "-g") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && (st.st_mode & S_ISGID)) ? 0 : 1;
        } else if (op == "-k") {
            struct stat st;
            return (stat(arg.c_str(), &st) == 0 && (st.st_mode & S_ISVTX)) ? 0 : 1;
        } else if (op == "!") {
            return arg.empty() ? 0 : 1;
        }
    }

    if (test_args.size() == 3) {
        if (test_args[0] == "!") {
            std::vector<std::string> neg_args = {"test", test_args[1], test_args[2]};
            return test_command(neg_args) == 0 ? 1 : 0;
        }

        const std::string& arg1 = test_args[0];
        const std::string& op = test_args[1];
        const std::string& arg2 = test_args[2];

        if (op == "=") {
            return arg1 == arg2 ? 0 : 1;
        } else if (op == "!=") {
            return arg1 != arg2 ? 0 : 1;
        } else if (op == "-eq") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 == n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-ne") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 != n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-lt") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 < n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-le") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 <= n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-gt") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 > n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-ge") {
            try {
                int n1 = std::stoi(arg1);
                int n2 = std::stoi(arg2);
                return n1 >= n2 ? 0 : 1;
            } catch (...) {
                return 1;
            }
        } else if (op == "-nt") {
            struct stat st1;
            struct stat st2;
            if (stat(arg1.c_str(), &st1) != 0 || stat(arg2.c_str(), &st2) != 0) {
                return 1;
            }
            return (st1.st_mtime > st2.st_mtime) ? 0 : 1;
        } else if (op == "-ot") {
            struct stat st1;
            struct stat st2;
            if (stat(arg1.c_str(), &st1) != 0 || stat(arg2.c_str(), &st2) != 0) {
                return 1;
            }
            return (st1.st_mtime < st2.st_mtime) ? 0 : 1;
        } else if (op == "-ef") {
            struct stat st1;
            struct stat st2;
            if (stat(arg1.c_str(), &st1) != 0 || stat(arg2.c_str(), &st2) != 0) {
                return 1;
            }
            return (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) ? 0 : 1;
        }
    }

    if (test_args.size() == 4 && test_args[0] == "!") {
        std::vector<std::string> neg_args = {"test", test_args[1], test_args[2], test_args[3]};
        return test_command(neg_args) == 0 ? 1 : 0;
    }

    return 1;
}
