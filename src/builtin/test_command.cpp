// test - check file types and compare values
// Based on GNU Coreutils test command
// Copyright (C) 1987-2025 Free Software Foundation, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// Authors: Kevin Braunsdorf, Matthew Bradburn

#include "test_command.h"
#include "builtin_help.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

// Platform compatibility for nanosecond timestamps
#ifdef __APPLE__
#define ST_MTIM_NSEC st_mtimespec.tv_nsec
#define ST_ATIM_NSEC st_atimespec.tv_nsec
#else
#define ST_MTIM_NSEC st_mtim.tv_nsec
#define ST_ATIM_NSEC st_atim.tv_nsec
#endif

namespace {

// Helper to check if path is a symbolic link
bool is_symlink(const char* path) {
    struct stat st;
    return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

// Test context
struct TestContext {
    std::vector<std::string> args;
    size_t pos;

    TestContext(const std::vector<std::string>& a) : args(a), pos(0) {
    }

    bool has_more() const {
        return pos < args.size();
    }
    const std::string& current() const {
        return args[pos];
    }
    void advance() {
        if (pos < args.size())
            pos++;
    }
    size_t remaining() const {
        return args.size() - pos;
    }
};

// Forward declarations
bool evaluate_expression(TestContext& ctx);
bool evaluate_or(TestContext& ctx);
bool evaluate_and(TestContext& ctx);
bool evaluate_term(TestContext& ctx);
bool evaluate_unary(TestContext& ctx);
bool evaluate_binary(TestContext& ctx);

// Unary operators
bool evaluate_unary(TestContext& ctx) {
    if (!ctx.has_more()) {
        return false;
    }

    const std::string& op = ctx.current();
    ctx.advance();

    if (!ctx.has_more()) {
        return false;  // Need an argument
    }

    const std::string& arg = ctx.current();
    ctx.advance();

    struct stat st;

    if (op == "-z") {
        return arg.empty();
    } else if (op == "-n") {
        return !arg.empty();
    } else if (op == "-e") {
        return access(arg.c_str(), F_OK) == 0;
    } else if (op == "-f") {
        return stat(arg.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    } else if (op == "-d") {
        return stat(arg.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    } else if (op == "-r") {
        return access(arg.c_str(), R_OK) == 0;
    } else if (op == "-w") {
        return access(arg.c_str(), W_OK) == 0;
    } else if (op == "-x") {
        return access(arg.c_str(), X_OK) == 0;
    } else if (op == "-s") {
        return stat(arg.c_str(), &st) == 0 && st.st_size > 0;
    } else if (op == "-L" || op == "-h") {
        return is_symlink(arg.c_str());
    } else if (op == "-p") {
        return stat(arg.c_str(), &st) == 0 && S_ISFIFO(st.st_mode);
    } else if (op == "-b") {
        return stat(arg.c_str(), &st) == 0 && S_ISBLK(st.st_mode);
    } else if (op == "-c") {
        return stat(arg.c_str(), &st) == 0 && S_ISCHR(st.st_mode);
    } else if (op == "-S") {
        return stat(arg.c_str(), &st) == 0 && S_ISSOCK(st.st_mode);
    } else if (op == "-u") {
        return stat(arg.c_str(), &st) == 0 && (st.st_mode & S_ISUID);
    } else if (op == "-g") {
        return stat(arg.c_str(), &st) == 0 && (st.st_mode & S_ISGID);
    } else if (op == "-k") {
        return stat(arg.c_str(), &st) == 0 && (st.st_mode & S_ISVTX);
    } else if (op == "-O") {
        return stat(arg.c_str(), &st) == 0 && st.st_uid == geteuid();
    } else if (op == "-G") {
        return stat(arg.c_str(), &st) == 0 && st.st_gid == getegid();
    } else if (op == "-N") {
        // File was modified since last read
        if (stat(arg.c_str(), &st) != 0)
            return false;
#ifdef __APPLE__
        return st.st_mtime > st.st_atime ||
               (st.st_mtime == st.st_atime && st.st_mtimespec.tv_nsec > st.st_atimespec.tv_nsec);
#else
        return st.st_mtime > st.st_atime ||
               (st.st_mtime == st.st_atime && st.st_mtim.tv_nsec > st.st_atim.tv_nsec);
#endif
    } else if (op == "-t") {
        // File descriptor is a terminal
        try {
            int fd = std::stoi(arg);
            return isatty(fd);
        } catch (...) {
            return false;
        }
    }

    return false;
}

// Binary operators
bool evaluate_binary(TestContext& ctx) {
    if (ctx.remaining() < 3) {
        return false;
    }

    const std::string& left = ctx.current();
    ctx.advance();
    const std::string& op = ctx.current();
    ctx.advance();
    const std::string& right = ctx.current();
    ctx.advance();

    // String comparisons
    if (op == "=" || op == "==") {
        return left == right;
    } else if (op == "!=") {
        return left != right;
    } else if (op == "<") {
        return left < right;
    } else if (op == ">") {
        return left > right;
    }

    // Integer comparisons
    try {
        long long left_val = std::stoll(left);
        long long right_val = std::stoll(right);

        if (op == "-eq") {
            return left_val == right_val;
        } else if (op == "-ne") {
            return left_val != right_val;
        } else if (op == "-lt") {
            return left_val < right_val;
        } else if (op == "-le") {
            return left_val <= right_val;
        } else if (op == "-gt") {
            return left_val > right_val;
        } else if (op == "-ge") {
            return left_val >= right_val;
        }
    } catch (...) {
        // If conversion fails, these operators return false
        if (op == "-eq" || op == "-ne" || op == "-lt" || op == "-le" || op == "-gt" ||
            op == "-ge") {
            return false;
        }
    }

    // File comparisons
    struct stat st1, st2;
    if (op == "-ef") {
        return stat(left.c_str(), &st1) == 0 && stat(right.c_str(), &st2) == 0 &&
               st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
    } else if (op == "-nt") {
        if (stat(left.c_str(), &st1) != 0 || stat(right.c_str(), &st2) != 0) {
            return false;
        }
#ifdef __APPLE__
        return st1.st_mtime > st2.st_mtime || (st1.st_mtime == st2.st_mtime &&
                                               st1.st_mtimespec.tv_nsec > st2.st_mtimespec.tv_nsec);
#else
        return st1.st_mtime > st2.st_mtime ||
               (st1.st_mtime == st2.st_mtime && st1.st_mtim.tv_nsec > st2.st_mtim.tv_nsec);
#endif
    } else if (op == "-ot") {
        if (stat(left.c_str(), &st1) != 0 || stat(right.c_str(), &st2) != 0) {
            return false;
        }
#ifdef __APPLE__
        return st1.st_mtime < st2.st_mtime || (st1.st_mtime == st2.st_mtime &&
                                               st1.st_mtimespec.tv_nsec < st2.st_mtimespec.tv_nsec);
#else
        return st1.st_mtime < st2.st_mtime ||
               (st1.st_mtime == st2.st_mtime && st1.st_mtim.tv_nsec < st2.st_mtim.tv_nsec);
#endif
    }

    return false;
}

// Check if string is a known binary operator
bool is_binary_op(const std::string& s) {
    return s == "=" || s == "==" || s == "!=" || s == "<" || s == ">" || s == "-eq" || s == "-ne" ||
           s == "-lt" || s == "-le" || s == "-gt" || s == "-ge" || s == "-ef" || s == "-nt" ||
           s == "-ot";
}

// Check if string is a known unary operator
bool is_unary_op(const std::string& s) {
    return s.length() == 2 && s[0] == '-' &&
           (s == "-z" || s == "-n" || s == "-e" || s == "-f" || s == "-d" || s == "-r" ||
            s == "-w" || s == "-x" || s == "-s" || s == "-L" || s == "-h" || s == "-p" ||
            s == "-b" || s == "-c" || s == "-S" || s == "-u" || s == "-g" || s == "-k" ||
            s == "-O" || s == "-G" || s == "-N" || s == "-t");
}

// Evaluate a term
bool evaluate_term(TestContext& ctx) {
    if (!ctx.has_more()) {
        return false;
    }

    // Handle negation
    bool negated = false;
    while (ctx.has_more() && ctx.current() == "!") {
        negated = !negated;
        ctx.advance();
    }

    if (!ctx.has_more()) {
        return false;
    }

    bool result;

    // Handle parentheses
    if (ctx.current() == "(") {
        ctx.advance();
        result = evaluate_expression(ctx);
        if (ctx.has_more() && ctx.current() == ")") {
            ctx.advance();
        }
    }
    // Check for binary operators
    else if (ctx.remaining() >= 3 && is_binary_op(ctx.args[ctx.pos + 1])) {
        result = evaluate_binary(ctx);
    }
    // Check for unary operators
    else if (is_unary_op(ctx.current())) {
        result = evaluate_unary(ctx);
    }
    // Just a string test (non-empty)
    else {
        result = !ctx.current().empty();
        ctx.advance();
    }

    return negated ? !result : result;
}

// Evaluate AND expression
bool evaluate_and(TestContext& ctx) {
    bool result = evaluate_term(ctx);

    while (ctx.has_more() && ctx.current() == "-a") {
        ctx.advance();
        result = result && evaluate_term(ctx);
    }

    return result;
}

// Evaluate OR expression
bool evaluate_or(TestContext& ctx) {
    bool result = evaluate_and(ctx);

    while (ctx.has_more() && ctx.current() == "-o") {
        ctx.advance();
        result = result || evaluate_and(ctx);
    }

    return result;
}

// Top-level expression evaluation
bool evaluate_expression(TestContext& ctx) {
    return evaluate_or(ctx);
}

}  // namespace

int test_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: test EXPRESSION",
                                   "   or: test",
                                   "   or: [ EXPRESSION ]",
                                   "   or: [ ]",
                                   "",
                                   "Evaluate conditional expressions.",
                                   "",
                                   "File tests:",
                                   "  -e FILE        FILE exists",
                                   "  -f FILE        FILE exists and is a regular file",
                                   "  -d FILE        FILE exists and is a directory",
                                   "  -h, -L FILE    FILE exists and is a symbolic link",
                                   "  -r FILE        FILE exists and read permission is granted",
                                   "  -w FILE        FILE exists and write permission is granted",
                                   "  -x FILE        FILE exists and execute permission is granted",
                                   "  -s FILE        FILE exists and has size greater than zero",
                                   "",
                                   "String tests:",
                                   "  -z STRING      STRING length is zero",
                                   "  -n STRING      STRING length is non-zero",
                                   "  STRING         equivalent to -n STRING",
                                   "  STR1 = STR2    strings are equal",
                                   "  STR1 != STR2   strings are not equal",
                                   "",
                                   "Integer tests:",
                                   "  INT1 -eq INT2  INT1 is equal to INT2",
                                   "  INT1 -ne INT2  INT1 is not equal to INT2",
                                   "  INT1 -lt INT2  INT1 is less than INT2",
                                   "  INT1 -le INT2  INT1 is less than or equal to INT2",
                                   "  INT1 -gt INT2  INT1 is greater than INT2",
                                   "  INT1 -ge INT2  INT1 is greater than or equal to INT2",
                                   "",
                                   "Logical operators:",
                                   "  ! EXPR         EXPR is false",
                                   "  EXPR1 -a EXPR2 both EXPR1 and EXPR2 are true",
                                   "  EXPR1 -o EXPR2 either EXPR1 or EXPR2 is true",
                                   "  ( EXPR )       value of EXPR"})) {
        return 0;
    }

    std::vector<std::string> test_args;

    // Handle [ command - remove closing ]
    if (args[0] == "[") {
        if (args.size() > 1 && args.back() == "]") {
            test_args = std::vector<std::string>(args.begin() + 1, args.end() - 1);
        } else {
            test_args = std::vector<std::string>(args.begin() + 1, args.end());
        }
    } else if (args[0] == "test") {
        test_args = std::vector<std::string>(args.begin() + 1, args.end());
    } else {
        test_args = std::vector<std::string>(args.begin(), args.end());
    }

    // Empty test returns false
    if (test_args.empty()) {
        return 1;
    }

    TestContext ctx(test_args);
    bool result = evaluate_expression(ctx);

    return result ? 0 : 1;
}
