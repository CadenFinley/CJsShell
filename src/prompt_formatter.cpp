#include "prompt_formatter.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>

#include "job_control.h"

namespace {

// Helper to get HOME directory
std::string get_home_dir() {
    const char* home = getenv("HOME");
    if (home) {
        return home;
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return pw->pw_dir;
    }
    return "";
}

// Helper to check if string starts with prefix
bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

std::string PromptFormatter::get_username() {
    const char* user = getenv("USER");
    if (user) {
        return user;
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        return pw->pw_name;
    }
    return "user";
}

std::string PromptFormatter::get_hostname(bool full) {
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "localhost";
    }

    std::string result(hostname);
    if (!full) {
        size_t dot_pos = result.find('.');
        if (dot_pos != std::string::npos) {
            result = result.substr(0, dot_pos);
        }
    }
    return result;
}

std::string PromptFormatter::get_cwd(bool basename_only) {
    std::filesystem::path cwd = std::filesystem::current_path();
    std::string cwd_str = cwd.string();

    if (!basename_only) {
        std::string home = get_home_dir();
        if (!home.empty() && starts_with(cwd_str, home)) {
            if (cwd_str == home) {
                return "~";
            }
            if (cwd_str.size() > home.size() && cwd_str[home.size()] == '/') {
                return "~" + cwd_str.substr(home.size());
            }
        }
        return cwd_str;
    }

    return cwd.filename().string();
}

std::string PromptFormatter::get_version(bool include_patch) {
    // Simple version for now - could be enhanced to read from build system
    if (include_patch) {
        return "3.10.2";
    }
    return "3.10";
}

std::string PromptFormatter::format_time(const std::string& format) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&now_c);

    if (!local_time) {
        return "";
    }

    char buffer[256];
    if (std::strftime(buffer, sizeof(buffer), format.c_str(), local_time) == 0) {
        return "";
    }

    return buffer;
}

std::string PromptFormatter::get_tty_name() {
    char* tty = ttyname(STDIN_FILENO);
    if (!tty) {
        return "?";
    }

    std::string tty_path(tty);
    size_t last_slash = tty_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        return tty_path.substr(last_slash + 1);
    }
    return tty_path;
}

int PromptFormatter::get_job_count() {
    return static_cast<int>(JobManager::instance().get_all_jobs().size());
}

char PromptFormatter::octal_to_char(const std::string& octal) {
    if (octal.empty() || octal.size() > 3) {
        return '\0';
    }

    int value = 0;
    for (char c : octal) {
        if (c < '0' || c > '7') {
            return '\0';
        }
        value = value * 8 + (c - '0');
    }

    if (value > 255) {
        return '\0';
    }

    return static_cast<char>(value);
}

std::string PromptFormatter::process_escapes(const std::string& format, bool for_isocline) {
    std::string result;
    result.reserve(format.size());

    bool in_escape = false;
    bool in_nonprint = false;
    std::string octal_buffer;

    for (size_t i = 0; i < format.size(); ++i) {
        char c = format[i];

        // Handle octal codes
        if (!octal_buffer.empty()) {
            if (c >= '0' && c <= '7' && octal_buffer.size() < 3) {
                octal_buffer += c;
                continue;
            } else {
                result += octal_to_char(octal_buffer);
                octal_buffer.clear();
            }
        }

        if (in_escape) {
            in_escape = false;

            switch (c) {
                case 'a':
                    result += '\a';
                    break;
                case 'd':
                    result += format_time("%a %b %d");
                    break;
                case 'D': {
                    // Handle \D{format}
                    if (i + 1 < format.size() && format[i + 1] == '{') {
                        size_t close = format.find('}', i + 2);
                        if (close != std::string::npos) {
                            std::string time_format = format.substr(i + 2, close - i - 2);
                            result += format_time(time_format);
                            i = close;
                        } else {
                            result += "\\D";
                        }
                    } else {
                        result += "\\D";
                    }
                    break;
                }
                case 'e':
                    result += '\033';
                    break;
                case 'h':
                    result += get_hostname(false);
                    break;
                case 'H':
                    result += get_hostname(true);
                    break;
                case 'j':
                    result += std::to_string(get_job_count());
                    break;
                case 'l':
                    result += get_tty_name();
                    break;
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    // Right-align marker - handled at higher level
                    result += "\r";
                    break;
                case 's':
                    result += "cjsh";
                    break;
                case 't':
                    result += format_time("%H:%M:%S");
                    break;
                case 'T':
                    result += format_time("%I:%M:%S");
                    break;
                case '@':
                    result += format_time("%I:%M %p");
                    break;
                case 'A':
                    result += format_time("%H:%M");
                    break;
                case 'u':
                    result += get_username();
                    break;
                case 'v':
                    result += get_version(false);
                    break;
                case 'V':
                    result += get_version(true);
                    break;
                case 'w':
                    result += get_cwd(false);
                    break;
                case 'W':
                    result += get_cwd(true);
                    break;
                case '!': {
                    // History number - get from isocline
                    const char* hist_str = getenv("CJSH_HISTNUM");
                    if (hist_str) {
                        result += hist_str;
                    } else {
                        result += "1";
                    }
                    break;
                }
                case '#': {
                    // Command number
                    const char* cmd_str = getenv("CJSH_CMDNUM");
                    if (cmd_str) {
                        result += cmd_str;
                    } else {
                        result += "1";
                    }
                    break;
                }
                case '$':
                    // # for root, $ for normal user
                    result += (geteuid() == 0) ? '#' : '$';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '[':
                    // Begin non-printing sequence for isocline
                    if (for_isocline) {
                        result += '[';
                        in_nonprint = true;
                    } else {
                        result += '[';
                    }
                    break;
                case ']':
                    // End non-printing sequence for isocline
                    if (for_isocline && in_nonprint) {
                        result += ']';
                        in_nonprint = false;
                    } else {
                        result += ']';
                    }
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                    // Start of octal code
                    octal_buffer += c;
                    break;
                default:
                    // Unknown escape - keep as is
                    result += '\\';
                    result += c;
                    break;
            }
        } else if (c == '\\') {
            in_escape = true;
        } else {
            // Don't escape '[' - let BBCode color tags work
            // Only literal brackets from \[ will be escaped during processing
            result += c;
        }
    }

    // Handle any remaining octal code
    if (!octal_buffer.empty()) {
        result += octal_to_char(octal_buffer);
    }

    return result;
}

bool PromptFormatter::has_right_aligned(const std::string& format) {
    // Check for \r escape sequence (not at the beginning)
    // Note: We're looking for the two-character sequence backslash-r, not CR (0x0D)
    size_t pos = 0;
    bool found_escape = false;

    while (pos < format.size()) {
        if (format[pos] == '\\' && pos + 1 < format.size()) {
            if (format[pos + 1] == 'r') {
                found_escape = true;
                break;
            }
            pos += 2;  // Skip escape sequence
        } else {
            pos++;
        }
    }

    return found_escape;
}

std::pair<std::string, std::string> PromptFormatter::split_prompt(const std::string& format) {
    std::string main_part;
    std::string right_part;

    bool in_escape = false;
    bool found_right = false;

    for (size_t i = 0; i < format.size(); ++i) {
        char c = format[i];

        if (in_escape) {
            in_escape = false;
            if (c == 'r' && !found_right) {
                found_right = true;
                continue;  // Don't include \r in output
            }

            if (!found_right) {
                main_part += '\\';
                main_part += c;
            } else {
                right_part += '\\';
                right_part += c;
            }
        } else if (c == '\\') {
            in_escape = true;
        } else {
            if (!found_right) {
                main_part += c;
            } else {
                right_part += c;
            }
        }
    }

    // Process escapes in both parts
    main_part = process_escapes(main_part, true);
    right_part = process_escapes(right_part, true);

    return {main_part, right_part};
}

std::string PromptFormatter::format_prompt(const std::string& format) {
    if (format.empty()) {
        return "$ ";
    }

    // Check if the prompt contains ANSI escape codes (from tools like starship)
    // If it does, return it as-is without processing escape sequences
    // This prevents corrupting prompts that are already formatted
    if (format.find("\033[") != std::string::npos || format.find("\x1b[") != std::string::npos) {
        return format;
    }

    // If no right-aligned part, just process escapes
    if (!has_right_aligned(format)) {
        return process_escapes(format, true);
    }

    // If has right-aligned, split and process separately
    // (caller will use split_prompt for proper handling)
    return process_escapes(format, true);
}
