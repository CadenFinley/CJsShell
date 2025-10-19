#include "printf_command.h"

#include "builtin_help.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "error_out.h"

namespace {

std::string format_printf_arg(const std::string& format_spec, const std::string& arg) {
    std::ostringstream result;

    if (format_spec.empty())
        return arg;

    size_t pos = 0;
    bool left_align = false;
    bool zero_pad = false;
    bool show_sign = false;
    [[maybe_unused]] bool space_sign = false;
    [[maybe_unused]] bool alt_form = false;
    int width = 0;
    int precision = -1;

    while (pos < format_spec.length()) {
        char c = format_spec[pos];
        if (c == '-') {
            left_align = true;
            pos++;
        } else if (c == '0') {
            zero_pad = true;
            pos++;
        } else if (c == '+') {
            show_sign = true;
            pos++;
        } else if (c == ' ') {
            space_sign = true;
            pos++;
        } else if (c == '#') {
            alt_form = true;
            pos++;
        } else {
            break;
        }
    }

    while (pos < format_spec.length() && format_spec[pos] >= '0' && format_spec[pos] <= '9') {
        width = width * 10 + (format_spec[pos] - '0');
        pos++;
    }

    if (pos < format_spec.length() && format_spec[pos] == '.') {
        pos++;
        precision = 0;
        while (pos < format_spec.length() && format_spec[pos] >= '0' && format_spec[pos] <= '9') {
            precision = precision * 10 + (format_spec[pos] - '0');
            pos++;
        }
    }

    char spec = format_spec.back();

    if (left_align) {
        result << std::left;
    } else {
        result << std::right;
    }

    if (width > 0) {
        result << std::setw(width);
    }

    switch (spec) {
        case 'd':
        case 'i': {
            long long val = 0;
            try {
                val = std::stoll(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0;
            }
            if (zero_pad && !left_align) {
                result << std::setfill('0');
            }
            if (show_sign && val >= 0) {
                result << '+';
            }
            result << val;
            break;
        }
        case 'o': {
            unsigned long long val = 0;
            try {
                val = std::stoull(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0;
            }
            if (zero_pad && !left_align) {
                result << std::setfill('0');
            }
            result << std::oct << val;
            break;
        }
        case 'x': {
            unsigned long long val = 0;
            try {
                val = std::stoull(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0;
            }
            if (zero_pad && !left_align) {
                result << std::setfill('0');
            }
            result << std::hex << std::nouppercase << val;
            break;
        }
        case 'X': {
            unsigned long long val = 0;
            try {
                val = std::stoull(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0;
            }
            if (zero_pad && !left_align) {
                result << std::setfill('0');
            }
            result << std::hex << std::uppercase << val;
            break;
        }
        case 'u': {
            unsigned long long val = 0;
            try {
                val = std::stoull(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0;
            }
            if (zero_pad && !left_align) {
                result << std::setfill('0');
            }
            result << val;
            break;
        }
        case 'f':
        case 'F': {
            double val = 0.0;
            try {
                val = std::stod(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0.0;
            }
            if (precision >= 0) {
                result << std::fixed << std::setprecision(precision) << val;
            } else {
                result << std::fixed << val;
            }
            break;
        }
        case 'e': {
            double val = 0.0;
            try {
                val = std::stod(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0.0;
            }
            if (precision >= 0) {
                result << std::scientific << std::nouppercase << std::setprecision(precision)
                       << val;
            } else {
                result << std::scientific << std::nouppercase << val;
            }
            break;
        }
        case 'E': {
            double val = 0.0;
            try {
                val = std::stod(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0.0;
            }
            if (precision >= 0) {
                result << std::scientific << std::uppercase << std::setprecision(precision) << val;
            } else {
                result << std::scientific << std::uppercase << val;
            }
            break;
        }
        case 'g':
        case 'G': {
            double val = 0.0;
            try {
                val = std::stod(arg.empty() ? "0" : arg);
            } catch (...) {
                val = 0.0;
            }
            if (precision >= 0) {
                result << std::setprecision(precision) << val;
            } else {
                result << val;
            }
            break;
        }
        case 'c': {
            if (!arg.empty()) {
                if (arg[0] >= '0' && arg[0] <= '9') {
                    int val = std::atoi(arg.c_str());
                    if (val >= 0 && val <= 255) {
                        result << static_cast<char>(val);
                    }
                } else {
                    result << arg[0];
                }
            }
            break;
        }
        case 's':
        default: {
            result << arg;
            break;
        }
    }

    return result.str();
}

std::string process_printf_escapes(const std::string& input) {
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
                case '"':
                    result += '"';
                    i++;
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7': {
                    int octal = 0;
                    int digits = 0;
                    while (i + 1 < input.length() && digits < 3 && input[i + 1] >= '0' &&
                           input[i + 1] <= '7') {
                        i++;
                        octal = octal * 8 + (input[i] - '0');
                        digits++;
                    }
                    result += static_cast<char>(octal);
                    break;
                }
                default:
                    result += next;
                    i++;
                    break;
            }
        } else {
            result += input[i];
        }
    }
    return result;
}

}  

int printf_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: printf FORMAT [ARG ...]",
                                   "Format and print ARG values according to FORMAT."})) {
        return 0;
    }
    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "printf", "missing format string", {}});
        return 1;
    }

    std::string format = process_printf_escapes(args[1]);
    std::vector<std::string> printf_args;

    for (size_t i = 2; i < args.size(); ++i) {
        printf_args.push_back(args[i]);
    }

    if (printf_args.empty()) {
        for (size_t i = 0; i < format.length(); ++i) {
            if (format[i] == '%' && i + 1 < format.length() && format[i + 1] == '%') {
                std::cout << '%';
                i++;
            } else {
                std::cout << format[i];
            }
        }
        return 0;
    }

    size_t arg_index = 0;

    while (arg_index < printf_args.size()) {
        bool consumed_arg_this_iteration = false;

        for (size_t i = 0; i < format.length(); ++i) {
            if (format[i] == '%' && i + 1 < format.length()) {
                if (format[i + 1] == '%') {
                    std::cout << '%';
                    i++;
                } else {
                    size_t spec_start = i;
                    i++;

                    while (i < format.length() &&
                           (format[i] == '-' || format[i] == '+' || format[i] == ' ' ||
                            format[i] == '#' || format[i] == '0')) {
                        i++;
                    }

                    while (i < format.length() && format[i] >= '0' && format[i] <= '9') {
                        i++;
                    }

                    if (i < format.length() && format[i] == '.') {
                        i++;
                        while (i < format.length() && format[i] >= '0' && format[i] <= '9') {
                            i++;
                        }
                    }

                    if (i < format.length()) {
                        i++;
                        std::string format_spec = format.substr(spec_start + 1, i - spec_start - 1);
                        std::string arg =
                            (arg_index < printf_args.size()) ? printf_args[arg_index] : "";

                        std::cout << format_printf_arg(format_spec, arg);
                        arg_index++;
                        consumed_arg_this_iteration = true;

                        if (arg_index >= printf_args.size()) {
                            while (i < format.length()) {
                                if (format[i] == '%' && i + 1 < format.length()) {
                                    if (format[i + 1] == '%') {
                                        std::cout << '%';
                                        i += 2;
                                    } else {
                                        size_t spec_start2 = i;
                                        i++;
                                        while (i < format.length() &&
                                               (format[i] == '-' || format[i] == '+' ||
                                                format[i] == ' ' || format[i] == '#' ||
                                                format[i] == '0')) {
                                            i++;
                                        }
                                        while (i < format.length() && format[i] >= '0' &&
                                               format[i] <= '9') {
                                            i++;
                                        }
                                        if (i < format.length() && format[i] == '.') {
                                            i++;
                                            while (i < format.length() && format[i] >= '0' &&
                                                   format[i] <= '9') {
                                                i++;
                                            }
                                        }
                                        if (i < format.length()) {
                                            i++;
                                            std::string format_spec2 =
                                                format.substr(spec_start2 + 1, i - spec_start2 - 1);
                                            std::cout << format_printf_arg(format_spec2, "");
                                        }
                                    }
                                } else {
                                    std::cout << format[i];
                                    i++;
                                }
                            }
                            return 0;
                        }

                        i--;
                    }
                }
            } else {
                std::cout << format[i];
            }
        }

        if (!consumed_arg_this_iteration) {
            break;
        }
    }

    return 0;
}
