#include "printf_command.h"

#include "builtin_help.h"

#include <cerrno>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "error_out.h"

namespace {

static int exit_status = 0;

static inline bool is_octal_digit(char c) {
    return c >= '0' && c <= '7';
}

static inline bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline int from_hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

static inline int from_octal(char c) {
    return c - '0';
}

static void verify_numeric(const char* s, char* end, const std::string& original) {
    if (s == end) {
        print_error(
            {ErrorType::INVALID_ARGUMENT, "printf", "expected a numeric value: " + original, {}});
        exit_status = 1;
    } else if (errno != 0) {
        print_error({ErrorType::INVALID_ARGUMENT, "printf", original, {}});
        exit_status = 1;
    } else if (*end != '\0') {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "printf",
                     "value not completely converted: " + original,
                     {}});
        exit_status = 1;
    }
}

static intmax_t vstrtoimax(const char* s) {
    if (!s || !*s)
        return 0;

    char* end;
    intmax_t val;

    if ((*s == '"' || *s == '\'') && *(s + 1)) {
        char quote = *s;
        unsigned char ch = *++s;
        val = ch;

        if (MB_CUR_MAX > 1 && *(s + 1) && *(s + 1) != quote) {
            mbstate_t mbstate;
            memset(&mbstate, 0, sizeof(mbstate));
            wchar_t wc;
            size_t slen = strlen(s);
            size_t bytes = mbrtowc(&wc, s, slen, &mbstate);
            if (bytes > 0 && bytes != (size_t)-1 && bytes != (size_t)-2) {
                val = wc;
                s += bytes - 1;
            }
        }

        ++s;
        if (*s != '\0' && *s != quote) {
            std::cerr
                << "printf: warning: character(s) following character constant have been ignored\n";
        }
    } else {
        errno = 0;
        val = strtoll(s, &end, 0);
        verify_numeric(s, end, s);
    }
    return val;
}

static uintmax_t vstrtoumax(const char* s) {
    if (!s || !*s)
        return 0;

    char* end;
    uintmax_t val;

    if ((*s == '"' || *s == '\'') && *(s + 1)) {
        char quote = *s;
        unsigned char ch = *++s;
        val = ch;

        if (MB_CUR_MAX > 1 && *(s + 1) && *(s + 1) != quote) {
            mbstate_t mbstate;
            memset(&mbstate, 0, sizeof(mbstate));
            wchar_t wc;
            size_t slen = strlen(s);
            size_t bytes = mbrtowc(&wc, s, slen, &mbstate);
            if (bytes > 0 && bytes != (size_t)-1 && bytes != (size_t)-2) {
                val = wc;
                s += bytes - 1;
            }
        }

        ++s;
        if (*s != '\0' && *s != quote) {
            std::cerr
                << "printf: warning: character(s) following character constant have been ignored\n";
        }
    } else {
        errno = 0;
        val = strtoull(s, &end, 0);
        verify_numeric(s, end, s);
    }
    return val;
}

static long double vstrtold(const char* s) {
    if (!s || !*s)
        return 0.0;

    char* end;
    long double val;

    if ((*s == '"' || *s == '\'') && *(s + 1)) {
        char quote = *s;
        unsigned char ch = *++s;
        val = ch;

        if (MB_CUR_MAX > 1 && *(s + 1) && *(s + 1) != quote) {
            mbstate_t mbstate;
            memset(&mbstate, 0, sizeof(mbstate));
            wchar_t wc;
            size_t slen = strlen(s);
            size_t bytes = mbrtowc(&wc, s, slen, &mbstate);
            if (bytes > 0 && bytes != (size_t)-1 && bytes != (size_t)-2) {
                val = wc;
                s += bytes - 1;
            }
        }

        ++s;
        if (*s != '\0' && *s != quote) {
            std::cerr
                << "printf: warning: character(s) following character constant have been ignored\n";
        }
    } else {
        errno = 0;
        val = strtold(s, &end);
        verify_numeric(s, end, s);
    }
    return val;
}

static void print_esc_char(char c) {
    switch (c) {
        case 'a':
            putchar('\a');
            break;
        case 'b':
            putchar('\b');
            break;
        case 'c':
            exit(0);
            break;
        case 'e':
            putchar('\x1B');
            break;
        case 'f':
            putchar('\f');
            break;
        case 'n':
            putchar('\n');
            break;
        case 'r':
            putchar('\r');
            break;
        case 't':
            putchar('\t');
            break;
        case 'v':
            putchar('\v');
            break;
        default:
            putchar(c);
            break;
    }
}

static void print_unicode_char(uint32_t code) {
    if (code <= 0x7F) {
        putchar(code);
    } else if (code <= 0x7FF) {
        putchar(0xC0 | (code >> 6));
        putchar(0x80 | (code & 0x3F));
    } else if (code <= 0xFFFF) {
        putchar(0xE0 | (code >> 12));
        putchar(0x80 | ((code >> 6) & 0x3F));
        putchar(0x80 | (code & 0x3F));
    } else if (code <= 0x10FFFF) {
        putchar(0xF0 | (code >> 18));
        putchar(0x80 | ((code >> 12) & 0x3F));
        putchar(0x80 | ((code >> 6) & 0x3F));
        putchar(0x80 | (code & 0x3F));
    }
}

static int print_esc(const char* escstart, bool octal_0) {
    const char* p = escstart + 1;
    int esc_value = 0;
    int esc_length;

    if (*p == 'x') {
        for (esc_length = 0, ++p; esc_length < 2 && is_hex_digit(*p); ++esc_length, ++p)
            esc_value = esc_value * 16 + from_hex(*p);
        if (esc_length == 0) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "printf",
                         "missing hexadecimal number in escape",
                         {}});
            exit(1);
        }
        putchar(esc_value);
    } else if (is_octal_digit(*p)) {
        for (esc_length = 0, p += octal_0 && *p == '0'; esc_length < 3 && is_octal_digit(*p);
             ++esc_length, ++p)
            esc_value = esc_value * 8 + from_octal(*p);
        putchar(esc_value);
    } else if (*p && strchr("\"\\abcefnrtv", *p)) {
        print_esc_char(*p++);
    } else if (*p == 'u' || *p == 'U') {
        char esc_char = *p;
        uint32_t uni_value = 0;

        for (esc_length = (esc_char == 'u' ? 4 : 8), ++p; esc_length > 0; --esc_length, ++p) {
            if (!is_hex_digit(*p)) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "printf",
                             "missing hexadecimal number in escape",
                             {}});
                exit(1);
            }
            uni_value = uni_value * 16 + from_hex(*p);
        }

        if (uni_value >= 0xD800 && uni_value <= 0xDFFF) {
            print_error(
                {ErrorType::INVALID_ARGUMENT, "printf", "invalid universal character name", {}});
            exit(1);
        }

        print_unicode_char(uni_value);
    } else {
        putchar('\\');
        if (*p) {
            putchar(*p);
            p++;
        }
    }
    return p - escstart - 1;
}

static void print_esc_string(const char* str) {
    for (; *str; str++)
        if (*str == '\\')
            str += print_esc(str, true);
        else
            putchar(*str);
}

static std::string shell_escape(const std::string& str) {
    std::ostringstream result;
    bool needs_quotes = false;

    for (char c : str) {
        if (!isalnum(c) && c != '_' && c != '-' && c != '/' && c != '.' && c != ',') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes && !str.empty()) {
        return str;
    }

    result << "'";
    for (char c : str) {
        if (c == '\'') {
            result << "'\\''";
        } else {
            result << c;
        }
    }
    result << "'";
    return result.str();
}

static void print_direc(const char* start, char conversion, bool have_field_width, int field_width,
                        bool have_precision, int precision, const char* argument) {
    std::ostringstream fmt;
    const char* p = start;

    fmt << "%";
    ++p;

    while (*p && strchr("-+ #0'I", *p)) {
        fmt << *p++;
    }

    if (have_field_width) {
        fmt << field_width;

        if (*p == '*') {
            ++p;
        } else {
            while (*p && *p >= '0' && *p <= '9') {
                ++p;
            }
        }
    } else {
        while (*p && *p >= '0' && *p <= '9') {
            fmt << *p++;
        }
    }

    if (*p == '.') {
        if (have_precision) {
            fmt << "." << precision;
            ++p;

            if (*p == '*') {
                ++p;
            } else {
                while (*p && *p >= '0' && *p <= '9') {
                    ++p;
                }
            }
        } else {
            fmt << *p++;
            while (*p && *p >= '0' && *p <= '9') {
                fmt << *p++;
            }
        }
    }

    while (*p && strchr("hlLjzt", *p)) {
        ++p;
    }

    switch (conversion) {
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X': {
            fmt << "ll" << conversion;
            if (conversion == 'd' || conversion == 'i') {
                long long val = argument ? vstrtoimax(argument) : 0;
                printf(fmt.str().c_str(), val);
            } else {
                unsigned long long val = argument ? vstrtoumax(argument) : 0;
                printf(fmt.str().c_str(), val);
            }
            break;
        }
        case 'a':
        case 'A':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G': {
            fmt << "L" << conversion;
            long double val = argument ? vstrtold(argument) : 0.0;
            printf(fmt.str().c_str(), val);
            break;
        }
        case 'c': {
            fmt << "c";
            int val;
            if (argument) {
                if ((*argument == '"' || *argument == '\'') && *(argument + 1)) {
                    val = vstrtoimax(argument);
                } else {
                    char* end;
                    errno = 0;
                    long num = strtol(argument, &end, 0);

                    if (errno == 0 && *end == '\0') {
                        val = num;
                    } else {
                        val = (unsigned char)argument[0];
                    }
                }
            } else {
                val = 0;
            }
            printf(fmt.str().c_str(), val);
            break;
        }
        case 's': {
            fmt << "s";
            const char* val = argument ? argument : "";
            printf(fmt.str().c_str(), val);
            break;
        }
    }
}

struct arg_cursor {
    const char* f;
    int curr_arg;
    int curr_s_arg;
    int end_arg;
    int direc_arg;
};

static arg_cursor get_curr_arg(int pos, arg_cursor ac) {
    int arg = 0;
    const char* f = ac.f;

    if (pos < 3 && isdigit(*f)) {
        int a = *f++ - '0';
        while (isdigit(*f)) {
            a = a * 10 + (*f++ - '0');
        }
        if (*f == '$') {
            arg = a;
        }
    }

    if (arg > 0) {
        arg--;
        ac.f = f + 1;
        if (pos == 0)
            ac.direc_arg = arg;
    } else {
        arg = (pos == 0                      ? (ac.direc_arg = -1)
               : pos < 3 || ac.direc_arg < 0 ? ++ac.curr_s_arg
                                             : ac.direc_arg);
    }

    if (arg >= 0) {
        ac.curr_arg = arg;
        if (arg > ac.end_arg)
            ac.end_arg = arg;
    }
    return ac;
}

static int print_formatted(const char* format, int argc, char** argv) {
    arg_cursor ac;
    ac.curr_arg = ac.curr_s_arg = ac.end_arg = ac.direc_arg = -1;
    const char* direc_start;
    bool have_field_width;
    int field_width = 0;
    bool have_precision;
    int precision = 0;
    bool ok[256] = {false};

    for (ac.f = format; *ac.f; ac.f++) {
        if (*ac.f == '%') {
            direc_start = ac.f;
            ac.f++;
            have_field_width = have_precision = false;

            if (*ac.f == '%') {
                putchar('%');
                continue;
            }

            ac = get_curr_arg(0, ac);

            if (*ac.f == 'b') {
                ac = get_curr_arg(3, ac);
                if (ac.curr_arg < argc)
                    print_esc_string(argv[ac.curr_arg]);
                continue;
            }

            if (*ac.f == 'q') {
                ac = get_curr_arg(3, ac);
                if (ac.curr_arg < argc) {
                    std::cout << shell_escape(argv[ac.curr_arg]);
                }
                continue;
            }

            memset(ok, 0, sizeof(ok));
            ok['a'] = ok['A'] = ok['c'] = ok['d'] = ok['e'] = ok['E'] = ok['f'] = ok['F'] =
                ok['g'] = ok['G'] = ok['i'] = ok['o'] = ok['s'] = ok['u'] = ok['x'] = ok['X'] =
                    true;

            while (*ac.f && strchr("-+ #0'I", *ac.f)) {
                switch (*ac.f) {
                    case '\'':
                        ok['a'] = ok['A'] = ok['c'] = ok['e'] = ok['E'] = ok['o'] = ok['s'] =
                            ok['x'] = ok['X'] = false;
                        break;
                    case '#':
                        ok['c'] = ok['d'] = ok['i'] = ok['s'] = ok['u'] = false;
                        break;
                    case '0':
                        ok['c'] = ok['s'] = false;
                        break;
                }
                ac.f++;
            }

            if (*ac.f == '*') {
                ac.f++;
                ac = get_curr_arg(1, ac);
                if (ac.curr_arg < argc) {
                    field_width = vstrtoimax(argv[ac.curr_arg]);
                } else {
                    field_width = 0;
                }
                have_field_width = true;
            } else {
                while (isdigit(*ac.f))
                    ac.f++;
            }

            if (*ac.f == '.') {
                ok['c'] = false;
                ac.f++;
                if (*ac.f == '*') {
                    ac.f++;
                    ac = get_curr_arg(2, ac);
                    if (ac.curr_arg < argc) {
                        precision = vstrtoimax(argv[ac.curr_arg]);
                        if (precision < 0)
                            precision = -1;
                    } else {
                        precision = 0;
                    }
                    have_precision = true;
                } else {
                    while (isdigit(*ac.f))
                        ac.f++;
                }
            }

            while (*ac.f && strchr("hlLjzt", *ac.f))
                ac.f++;

            unsigned char conversion = *ac.f;
            if (!ok[conversion]) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             "printf",
                             "invalid conversion specification",
                             {}});
                return -1;
            }

            ac = get_curr_arg(3, ac);

            print_direc(direc_start, conversion, have_field_width, field_width, have_precision,
                        precision, ac.curr_arg < argc ? argv[ac.curr_arg] : nullptr);

        } else if (*ac.f == '\\') {
            ac.f += print_esc(ac.f, false);
        } else {
            putchar(*ac.f);
        }
    }

    return ac.end_arg + 1;
}

}  // namespace

int printf_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args,
            {"Usage: printf FORMAT [ARGUMENT]...",
             "Format and print ARGUMENT(s) according to FORMAT.",
             "",
             "FORMAT controls the output as in C printf. Interpreted sequences:",
             "  \\\"       double quote",
             "  \\\\       backslash",
             "  \\a       alert (BEL)",
             "  \\b       backspace",
             "  \\c       produce no further output",
             "  \\e       escape",
             "  \\f       form feed",
             "  \\n       new line",
             "  \\r       carriage return",
             "  \\t       horizontal tab",
             "  \\v       vertical tab",
             "  \\NNN     byte with octal value NNN (1 to 3 digits)",
             "  \\xHH     byte with hexadecimal value HH (1 to 2 digits)",
             "  \\uHHHH   Unicode character with hex value HHHH (4 digits)",
             "  \\UHHHHHHHH Unicode character with hex value HHHHHHHH (8 digits)",
             "",
             "  %%       a single %",
             "  %b       ARGUMENT as a string with '\\' escapes interpreted",
             "  %q       ARGUMENT is printed in shell-quoted format",
             "",
             "All C format specifications ending with diouxXfeEgGaAcs are supported,",
             "with ARGUMENTs converted to proper type first. Variable widths are handled."})) {
        return 0;
    }

    if (args.size() < 2) {
        print_error({ErrorType::INVALID_ARGUMENT, "printf", "missing operand", {}});
        return 1;
    }

    exit_status = 0;

    std::string format = args[1];
    std::vector<std::string> argv_storage;
    std::vector<char*> argv_ptrs;

    for (size_t i = 2; i < args.size(); i++) {
        argv_storage.push_back(args[i]);
    }

    for (size_t i = 0; i < argv_storage.size(); i++) {
        argv_ptrs.push_back(const_cast<char*>(argv_storage[i].c_str()));
    }

    int argc = argv_ptrs.size();
    char** argv = argv_ptrs.empty() ? nullptr : argv_ptrs.data();

    int args_used;
    do {
        args_used = print_formatted(format.c_str(), argc, argv);
        if (args_used < 0)
            return 1;
        argc -= args_used;
        argv += args_used;
    } while (args_used > 0 && argc > 0);

    fflush(stdout);
    return exit_status;
}
