#include "echo_command.h"

#include "builtin_help.h"

#include <unistd.h>

#include <iostream>
#include <string>

namespace {

std::string process_escape_sequences(const std::string& input) {
    std::string result;
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '\\' && i + 1 < input.length()) {
            char next = input[i + 1];
            switch (next) {
                case 'a':
                    result += '\a';
                    i++;
                    break;
                case 'b':
                    result += '\b';
                    i++;
                    break;
                case 'f':
                    result += '\f';
                    i++;
                    break;
                case 'n':
                    result += '\n';
                    i++;
                    break;
                case 'r':
                    result += '\r';
                    i++;
                    break;
                case 't':
                    result += '\t';
                    i++;
                    break;
                case 'v':
                    result += '\v';
                    i++;
                    break;
                case '\\':
                    result += '\\';
                    i++;
                    break;
                case '0': {
                    if (i + 4 < input.length() && input[i + 2] >= '0' && input[i + 2] <= '7' &&
                        input[i + 3] >= '0' && input[i + 3] <= '7' && input[i + 4] >= '0' &&
                        input[i + 4] <= '7') {
                        int octal = (input[i + 2] - '0') * 64 + (input[i + 3] - '0') * 8 +
                                    (input[i + 4] - '0');
                        result += static_cast<char>(octal);
                        i += 4;
                    } else {
                        result += input[i];
                    }
                    break;
                }
                default:
                    result += input[i];
                    break;
            }
        } else {
            result += input[i];
        }
    }
    return result;
}

}  

int echo_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args,
            {"Usage: echo [-n] [-e|-E] [STRING ...]", "Display arguments separated by spaces.",
             "-n suppresses the trailing newline, -e enables escapes, -E disables them."})) {
        return 0;
    }
    std::vector<std::string> echo_args = args;
    bool redirect_to_stderr = false;
    bool suppress_newline = false;
    bool interpret_escapes = false;

    if (args.size() > 1 && args.back() == ">&2") {
        redirect_to_stderr = true;
        echo_args.pop_back();
    }

    size_t start_idx = 1;
    while (start_idx < echo_args.size() && echo_args[start_idx][0] == '-' &&
           echo_args[start_idx].length() > 1) {
        const std::string& flag = echo_args[start_idx];

        if (flag == "-n") {
            suppress_newline = true;
        } else if (flag == "-e") {
            interpret_escapes = true;
        } else if (flag == "-E") {
            interpret_escapes = false;
        } else if (flag == "--") {
            start_idx++;
            break;
        } else {
            break;
        }
        start_idx++;
    }

    bool first = true;

    for (size_t i = start_idx; i < echo_args.size(); ++i) {
        if (!first) {
            if (redirect_to_stderr) {
                std::cerr << " ";
            } else {
                std::cout << " ";
            }
        }

        std::string output = echo_args[i];
        if (interpret_escapes) {
            output = process_escape_sequences(output);
        }

        if (redirect_to_stderr) {
            std::cerr << output;
        } else {
            std::cout << output;
        }
        first = false;
    }

    if (!suppress_newline) {
        if (redirect_to_stderr) {
            std::cerr << "\n";
            std::cerr.flush();
        } else {
            std::cout << "\n";
            std::cout.flush();
        }
    } else {
        if (redirect_to_stderr) {
            std::cerr.flush();
        } else {
            std::cout.flush();
        }
    }

    return 0;
}
