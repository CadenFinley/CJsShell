#include "prompt.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>

#include "cjsh.h"
#include "isocline.h"
#include "job_control.h"
#include "shell.h"
#include "token_constants.h"

namespace prompt {
namespace {

std::string get_env(const char* name, const char* fallback = nullptr) {
    const char* value = std::getenv(name);
    if (value != nullptr && value[0] != '\0') {
        return value;
    }
    if (fallback != nullptr) {
        return fallback;
    }
    return {};
}

std::string get_shell_name() {
    std::string shell_name = get_env("0");
    if (shell_name.empty() && !startup_args().empty()) {
        shell_name = startup_args().front();
    }
    if (shell_name.empty()) {
        shell_name = "cjsh";
    }
    auto pos = shell_name.find_last_of('/');
    if (pos != std::string::npos) {
        shell_name = shell_name.substr(pos + 1);
    }
    return shell_name;
}

std::string get_username() {
    std::string username = get_env("USER");
    if (!username.empty()) {
        return username;
    }
    uid_t uid = geteuid();
    passwd* pw = getpwuid(uid);
    if (pw != nullptr && pw->pw_name != nullptr) {
        return pw->pw_name;
    }
    return {};
}

std::string get_hostname(bool full) {
    char buffer[256] = {0};
    if (gethostname(buffer, sizeof(buffer)) != 0) {
        return {};
    }
    std::string host(buffer);
    if (!full) {
        auto pos = host.find('.');
        if (pos != std::string::npos) {
            host.resize(pos);
        }
    }
    return host;
}

bool terminal_supports_color() {
    if (isatty(STDOUT_FILENO) == 0) {
        return false;
    }

    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }

    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm != nullptr) {
        std::string colorterm_lower;
        for (const char* p = colorterm; *p != '\0'; ++p) {
            colorterm_lower.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
        }
        if (colorterm_lower.find("nocolor") != std::string::npos ||
            colorterm_lower.find("monochrome") != std::string::npos) {
            return false;
        }
        if (!colorterm_lower.empty()) {
            return true;
        }
    }

    const char* term = std::getenv("TERM");
    if (term == nullptr) {
        return true;
    }

    std::string term_lower;
    for (const char* p = term; *p != '\0'; ++p) {
        term_lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }

    const bool unsupported = term_lower.find("dumb") != std::string::npos ||
                             term_lower.find("cons25") != std::string::npos ||
                             term_lower.find("emacs") != std::string::npos ||
                             term_lower.find("nocolor") != std::string::npos ||
                             term_lower.find("monochrome") != std::string::npos;

    return !unsupported;
}

std::string format_time(const char* fmt) {
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    (void)localtime_r(&now, &tm_now);

    char buffer[256];
    size_t written = std::strftime(buffer, sizeof(buffer), fmt, &tm_now);
    if (written == 0) {
        return {};
    }
    return std::string(buffer, written);
}

std::string get_cwd(bool abbreviate_home, bool basename_only) {
    std::error_code ec;
    std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec) {
        return {};
    }
    std::string path = cwd.string();

    std::string home = get_env("HOME");
    if (abbreviate_home && !home.empty()) {
        if (path == home) {
            path = "~";
        } else if (path.rfind(home + "/", 0) == 0) {
            path = "~" + path.substr(home.size());
        }
    }

    if (!basename_only) {
        return path;
    }

    if (path == "~") {
        return path;
    }

    std::filesystem::path path_obj(path);
    std::string base = path_obj.filename().string();
    if (base.empty()) {
        base = path_obj.root_path().string();
    }
    if (abbreviate_home && !home.empty() && path.rfind(home + "/", 0) == 0) {
        if (path == home) {
            return "~";
        }
        base = path.substr(path.find_last_of('/') + 1);
    }
    return base;
}

std::string get_terminal_name() {
    const char* tty = ttyname(STDIN_FILENO);
    if (tty == nullptr) {
        return {};
    }
    std::string name(tty);
    auto pos = name.find_last_of('/');
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    return name;
}

std::string get_version_short() {
    std::string version = get_version();
    auto pos = version.find(' ');
    if (pos != std::string::npos) {
        version = version.substr(0, pos);
    }
    pos = version.find_last_of('.');
    if (pos != std::string::npos) {
        return version.substr(0, pos);
    }
    return version;
}

std::string get_version_full() {
    return get_version();
}

std::string get_exit_status() {
    return get_env("?");
}

std::string get_job_count() {
    size_t count = JobManager::instance().get_all_jobs().size();
    return std::to_string(count);
}

char prompt_dollar() {
    if (geteuid() == 0) {
        return '#';
    }
    return '$';
}

char to_ascii(std::uint8_t value) {
    return static_cast<char>(value);
}

std::string expand_prompt_string(const std::string& templ) {
    std::string result;
    result.reserve(templ.size() + 16);

    for (size_t i = 0; i < templ.size(); ++i) {
        char ch = templ[i];
        if (ch != '\\') {
            result.push_back(ch);
            continue;
        }

        ++i;
        if (i >= templ.size()) {
            result.push_back('\\');
            break;
        }

        char code = templ[i];
        switch (code) {
            case 'a':
                result.push_back('\a');
                break;
            case 'd':
                result += format_time("%a %b %e");
                break;
            case 'D': {
                if (i + 1 < templ.size() && templ[i + 1] == '{') {
                    size_t closing = templ.find('}', i + 2);
                    if (closing != std::string::npos) {
                        std::string fmt = templ.substr(i + 2, closing - (i + 2));
                        result += format_time(fmt.c_str());
                        i = closing;
                        break;
                    }
                }
                result.push_back('D');
                break;
            }
            case 'e':
            case 'E':
                result.push_back('\033');
                break;
            case 'h':
                result += get_hostname(false);
                break;
            case 'H':
                result += get_hostname(true);
                break;
            case 'j':
                result += get_job_count();
                break;
            case 'l':
                result += get_terminal_name();
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 's':
                result += get_shell_name();
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
                result += get_version_short();
                break;
            case 'V':
                result += get_version_full();
                break;
            case 'w':
                result += get_cwd(true, false);
                break;
            case 'W':
                result += get_cwd(true, true);
                break;
            case '$':
                result.push_back(prompt_dollar());
                break;
            case '?':
                result += get_exit_status();
                break;
            case '\\':
                result.push_back('\\');
                break;
            case '[':
            case ']':
                break;
            default: {
                if (code >= '0' && code <= '7') {
                    int value = code - '0';
                    int digits = 1;
                    while (digits < 3 && (i + 1) < templ.size()) {
                        char next = templ[i + 1];
                        if (next < '0' || next > '7') {
                            break;
                        }
                        value = (value * 8) + (next - '0');
                        ++i;
                        ++digits;
                    }
                    result.push_back(to_ascii(static_cast<std::uint8_t>(value & 0xFF)));
                } else {
                    result.push_back(code);
                }
                break;
            }
        }
    }

    return result;
}

std::string get_ps(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value != nullptr) {
        return value;
    }
    return fallback;
}

}  // namespace

std::string default_primary_prompt_template() {
    return "[!red][[/red][yellow]\\u[/yellow][green]@[/green][blue]\\h[/blue] "
           "[color=#ff69b4]\\w[/color][!red]][/red][!b] \\$ [/b]";
}

std::string default_right_prompt_template() {
    return "[ic-hint]\\A[/ic-hint]";
}

std::string render_primary_prompt() {
    std::string ps1 = get_ps("PS1", default_primary_prompt_template());
    return expand_prompt_string(ps1);
}

std::string render_right_prompt() {
    if (const char* rprompt = std::getenv("RPROMPT"); rprompt != nullptr) {
        return expand_prompt_string(rprompt);
    }

    std::string rps1 = get_ps("RPS1", default_right_prompt_template());
    return expand_prompt_string(rps1);
}

std::string render_secondary_prompt() {
    const char* ps2 = std::getenv("PS2");
    if (ps2 == nullptr || ps2[0] == '\0') {
        return {};
    }
    return expand_prompt_string(ps2);
}

void execute_prompt_command() {
    if (!g_shell) {
        return;
    }
    std::string command = get_env("PROMPT_COMMAND");
    if (command.empty()) {
        return;
    }
    g_shell->execute(command);
}

void initialize_colors() {
    if (!config::colors_enabled) {
        ic_enable_color(false);
        return;
    }

    if (!terminal_supports_color()) {
        config::colors_enabled = false;
        ic_enable_color(false);
        return;
    }

    ic_enable_color(true);

    if (!config::syntax_highlighting_enabled) {
        return;
    }

    for (const auto& pair : token_constants::default_styles()) {
        std::string style_name = pair.first;
        if (style_name.rfind("ic-", 0) != 0) {
            style_name = "cjsh-";
            style_name += pair.first;
        }
        ic_style_def(style_name.c_str(), pair.second.c_str());
        ic_style_def("ic-prompt", "white");
    }
}
}  // namespace prompt
