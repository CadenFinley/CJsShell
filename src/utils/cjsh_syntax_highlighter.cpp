#include "cjsh_syntax_highlighter.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "shell.h"

const std::unordered_set<std::string> SyntaxHighlighter::basic_unix_commands_ = {
    "cat",  "mv",    "cp",    "rm", "mkdir", "rmdir", "touch",  "grep",
    "find", "chmod", "chown", "ps", "man",   "which", "whereis"};
std::unordered_set<std::string> SyntaxHighlighter::external_executables_;
const std::unordered_set<std::string> SyntaxHighlighter::command_operators_ = {"&&", "||", "|",
                                                                               ";"};

const std::unordered_set<std::string> SyntaxHighlighter::shell_keywords_ = {
    "if",     "then",  "else", "elif", "fi",   "case",     "in",     "esac",
    "while",  "until", "for",  "do",   "done", "function", "select", "time",
    "coproc", "{",     "}",    "[[",   "]]",   "(",        ")",      ":"};

const std::unordered_set<std::string> SyntaxHighlighter::shell_built_ins_ = {
    "echo",     "printf", "pwd",     "cd",      "ls",       "alias",    "export",      "unalias",
    "unset",    "set",    "shift",   "break",   "continue", "return",   "ai",          "source",
    ".",        "theme",  "plugin",  "help",    "approot",  "aihelp",   "version",     "uninstall",
    "eval",     "syntax", "history", "exit",    "quit",     "terminal", "prompt_test", "test",
    "[",        "exec",   "trap",    "jobs",    "fg",       "bg",       "wait",        "kill",
    "readonly", "read",   "umask",   "getopts", "times",    "type",     "hash"};

void SyntaxHighlighter::initialize() {
    for (const auto& e : cjsh_filesystem::read_cached_executables()) {
        external_executables_.insert(e.filename().string());
    }
}

void SyntaxHighlighter::refresh_executables_cache() {
    external_executables_.clear();
    for (const auto& e : cjsh_filesystem::read_cached_executables()) {
        external_executables_.insert(e.filename().string());
    }
}

bool SyntaxHighlighter::is_shell_keyword(const std::string& token) {
    return shell_keywords_.count(token) > 0;
}

bool SyntaxHighlighter::is_shell_builtin(const std::string& token) {
    return shell_built_ins_.count(token) > 0;
}

bool SyntaxHighlighter::is_variable_reference(const std::string& token) {
    if (token.empty())
        return false;

    if (token[0] == '$')
        return true;

    size_t eq_pos = token.find('=');
    if (eq_pos != std::string::npos && eq_pos > 0) {
        std::string var_name = token.substr(0, eq_pos);

        if (!var_name.empty() && (std::isalpha(var_name[0]) || var_name[0] == '_')) {
            return true;
        }
    }

    return false;
}

bool SyntaxHighlighter::is_quoted_string(const std::string& token, char& quote_type) {
    if (token.length() < 2)
        return false;

    char first = token[0];
    char last = token[token.length() - 1];

    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        quote_type = first;
        return true;
    }

    return false;
}

bool SyntaxHighlighter::is_redirection_operator(const std::string& token) {
    static const std::unordered_set<std::string> redirection_ops = {
        ">",  ">>",  "<",  "<<",  "<<<",  "&>",   "&>>", "<&", ">&", "|&",
        "2>", "2>>", "1>", "1>>", "2>&1", "1>&2", ">&2", "<>", "1<", "2<",
        "0<", "0>",  "3>", "4>",  "5>",   "6>",   "7>",  "8>", "9>"};
    return redirection_ops.count(token) > 0;
}

bool SyntaxHighlighter::is_glob_pattern(const std::string& token) {
    return token.find_first_of("*?[]{}") != std::string::npos;
}

bool SyntaxHighlighter::is_function_definition(const std::string& input, size_t& func_name_start,
                                               size_t& func_name_end) {
    func_name_start = 0;
    func_name_end = 0;

    std::string trimmed = input;

    size_t first_non_space = trimmed.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return false;
    }

    if (trimmed.substr(first_non_space, 8) == "function") {
        size_t name_start = first_non_space + 8;

        while (name_start < trimmed.length() && std::isspace(trimmed[name_start])) {
            name_start++;
        }

        if (name_start >= trimmed.length()) {
            return false;
        }

        size_t name_end = name_start;
        while (name_end < trimmed.length() && !std::isspace(trimmed[name_end]) &&
               trimmed[name_end] != '{') {
            name_end++;
        }

        if (name_end > name_start) {
            func_name_start = name_start;
            func_name_end = name_end;
            return true;
        }
    }

    size_t paren_pos = trimmed.find("()");
    if (paren_pos != std::string::npos && paren_pos >= first_non_space) {
        size_t name_start = first_non_space;
        size_t name_end = paren_pos;

        while (name_end > name_start && std::isspace(trimmed[name_end - 1])) {
            name_end--;
        }

        if (name_end > name_start) {
            std::string func_name = trimmed.substr(name_start, name_end - name_start);
            if (!func_name.empty() && func_name.find(' ') == std::string::npos &&
                func_name.find('\t') == std::string::npos) {
                func_name_start = name_start;
                func_name_end = name_end;
                return true;
            }
        }
    }

    return false;
}

void SyntaxHighlighter::highlight_quotes_and_variables(ic_highlight_env_t* henv, const char* input,
                                                       size_t start, size_t length) {
    if (length == 0)
        return;

    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (size_t i = 0; i < length; ++i) {
        char c = input[start + i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\' && (!in_single_quote)) {
            escaped = true;
            continue;
        }

        if (c == '\'' && !in_double_quote) {
            if (!in_single_quote) {
                in_single_quote = true;

                size_t quote_start = i;
                size_t quote_end = i + 1;
                while (quote_end < length && input[start + quote_end] != '\'') {
                    quote_end++;
                }
                if (quote_end < length) {
                    quote_end++;
                    ic_highlight(henv, start + quote_start, quote_end - quote_start, "cjsh-string");
                    i = quote_end - 1;
                    in_single_quote = false;
                }
            }
        } else if (c == '"' && !in_single_quote) {
            if (!in_double_quote) {
                in_double_quote = true;

                size_t quote_start = i;
                size_t quote_end = i + 1;
                bool quote_escaped = false;
                while (quote_end < length) {
                    char qc = input[start + quote_end];
                    if (quote_escaped) {
                        quote_escaped = false;
                    } else if (qc == '\\') {
                        quote_escaped = true;
                    } else if (qc == '"') {
                        break;
                    }
                    quote_end++;
                }
                if (quote_end < length) {
                    quote_end++;
                    ic_highlight(henv, start + quote_start, quote_end - quote_start, "cjsh-string");
                    i = quote_end - 1;
                    in_double_quote = false;
                }
            }
        } else if (c == '$' && !in_single_quote) {
            size_t var_start = i;
            size_t var_end = i + 1;

            if (var_end < length && input[start + var_end] == '{') {
                var_end++;
                while (var_end < length && input[start + var_end] != '}') {
                    var_end++;
                }
                if (var_end < length) {
                    var_end++;
                }
            } else {
                while (var_end < length) {
                    char vc = input[start + var_end];
                    if (std::isalnum(vc) || vc == '_' ||
                        (var_end == var_start + 1 && std::isdigit(vc))) {
                        var_end++;
                    } else {
                        break;
                    }
                }
            }

            if (var_end > var_start + 1) {
                ic_highlight(henv, start + var_start, var_end - var_start, "cjsh-variable");
                i = var_end - 1;
            }
        }
    }
}

void SyntaxHighlighter::highlight(ic_highlight_env_t* henv, const char* input, void*) {
    if (!g_shell->get_menu_active() && input[0] != ':') {
        return;
    }
    size_t len = std::strlen(input);

    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    for (size_t i = 0; i < len; ++i) {
        char c = input[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\' && (!in_quotes || quote_char != '\'')) {
            escaped = true;
            continue;
        }

        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
        } else if (c == quote_char && in_quotes) {
            in_quotes = false;
            quote_char = '\0';
        }

        if (!in_quotes && c == '#') {
            ic_highlight(henv, i, len - i, "cjsh-comment");
            break;
        }
    }

    size_t func_name_start, func_name_end;
    std::string input_str(input, len);
    if (is_function_definition(input_str, func_name_start, func_name_end)) {
        ic_highlight(henv, func_name_start, func_name_end - func_name_start,
                     "cjsh-function-definition");

        size_t paren_pos = input_str.find("()", func_name_end);
        if (paren_pos != std::string::npos && paren_pos < len) {
            ic_highlight(henv, paren_pos, 2, "cjsh-function-definition");
        }

        size_t brace_pos = input_str.find("{");
        if (brace_pos != std::string::npos && brace_pos < len) {
            ic_highlight(henv, brace_pos, 1, "cjsh-operator");
        }
        return;
    }

    if (!g_shell->get_menu_active() && input[0] == ':') {
        ic_highlight(henv, 0, 1, "cjsh-colon");

        size_t i = 0;
        while (i < len && !std::isspace((unsigned char)input[i]))
            ++i;
        std::string token(input, i);

        if (token.size() > 1) {
            std::string sub = token.substr(1);
            if (sub.rfind("./", 0) == 0) {
                if (!std::filesystem::exists(sub) || !std::filesystem::is_regular_file(sub)) {
                    ic_highlight(henv, 1, i - 1, "cjsh-unknown-command");
                } else {
                    ic_highlight(henv, 1, i - 1, "cjsh-installed");
                }
            } else if (is_shell_keyword(sub)) {
                ic_highlight(henv, 1, i - 1, "cjsh-keyword");
            } else if (is_shell_builtin(sub)) {
                ic_highlight(henv, 1, i - 1, "cjsh-builtin");
            } else {
                auto cmds = g_shell->get_available_commands();
                if (std::find(cmds.begin(), cmds.end(), sub) != cmds.end()) {
                    ic_highlight(henv, 1, i - 1, "cjsh-builtin");
                } else if (basic_unix_commands_.count(sub) > 0) {
                    ic_highlight(henv, 1, i - 1, "cjsh-system");
                } else if (external_executables_.count(sub) > 0) {
                    ic_highlight(henv, 1, i - 1, "cjsh-installed");
                } else {
                    ic_highlight(henv, 1, i - 1, "cjsh-unknown-command");
                }
            }
        }
        return;
    }

    size_t pos = 0;
    while (pos < len) {
        size_t cmd_end = pos;
        while (cmd_end < len) {
            if ((cmd_end + 1 < len && input[cmd_end] == '&' && input[cmd_end + 1] == '&') ||
                (cmd_end + 1 < len && input[cmd_end] == '|' && input[cmd_end + 1] == '|') ||
                input[cmd_end] == '|' || input[cmd_end] == ';') {
                break;
            }
            cmd_end++;
        }

        size_t cmd_start = pos;
        while (cmd_start < cmd_end && std::isspace((unsigned char)input[cmd_start])) {
            cmd_start++;
        }

        std::string cmd_str(input + cmd_start, cmd_end - cmd_start);

        size_t token_end = 0;
        while (token_end < cmd_str.length() && !std::isspace((unsigned char)cmd_str[token_end])) {
            token_end++;
        }

        std::string token = token_end > 0 ? cmd_str.substr(0, token_end) : "";

        bool is_sudo_command = (token == "sudo");

        if (!token.empty()) {
            if (token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0 ||
                token.rfind("~/", 0) == 0 || token.rfind("-/", 0) == 0 || token[0] == '/' ||
                token.find('/') != std::string::npos) {
                std::string path_to_check = token;
                if (token.rfind("~/", 0) == 0) {
                    path_to_check = cjsh_filesystem::g_user_home_path.string() + token.substr(1);
                } else if (token.rfind("-/", 0) == 0) {
                    std::string prev_dir = g_shell->get_previous_directory();
                    if (!prev_dir.empty()) {
                        path_to_check = prev_dir + token.substr(1);
                    }
                } else if (token[0] != '/' && token.rfind("./", 0) != 0 &&
                           token.rfind("../", 0) != 0 && token.rfind("~/", 0) != 0 &&
                           token.rfind("-/", 0) != 0) {
                    path_to_check = std::filesystem::current_path().string() + "/" + token;
                }
                if (std::filesystem::exists(path_to_check)) {
                    ic_highlight(henv, cmd_start, token_end, "cjsh-installed");
                } else {
                    ic_highlight(henv, cmd_start, token_end, "cjsh-unknown-command");
                }
            } else if (is_shell_keyword(token)) {
                ic_highlight(henv, cmd_start, token_end, "cjsh-keyword");
            } else if (is_shell_builtin(token)) {
                ic_highlight(henv, cmd_start, token_end, "cjsh-builtin");
            } else {
                auto cmds = g_shell->get_available_commands();
                if (std::find(cmds.begin(), cmds.end(), token) != cmds.end()) {
                    ic_highlight(henv, cmd_start, token_end, "cjsh-builtin");
                } else if (basic_unix_commands_.count(token) > 0) {
                    ic_highlight(henv, cmd_start, token_end, "cjsh-system");
                } else if (external_executables_.count(token) > 0) {
                    ic_highlight(henv, cmd_start, token_end, "cjsh-installed");
                } else {
                    ic_highlight(henv, cmd_start, token_end, "cjsh-unknown-command");
                }
            }
        }

        bool is_cd_command = (token == "cd");
        size_t arg_start = token_end;

        while (arg_start < cmd_str.length()) {
            while (arg_start < cmd_str.length() &&
                   std::isspace((unsigned char)cmd_str[arg_start])) {
                arg_start++;
            }
            if (arg_start >= cmd_str.length())
                break;

            size_t arg_end = arg_start;
            while (arg_end < cmd_str.length() && !std::isspace((unsigned char)cmd_str[arg_end])) {
                arg_end++;
            }

            std::string arg = cmd_str.substr(arg_start, arg_end - arg_start);

            if (is_redirection_operator(arg)) {
                ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start, "cjsh-operator");
            }

            else if (is_variable_reference(arg)) {
                ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start, "cjsh-variable");
            }

            else {
                char quote_type;
                if (is_quoted_string(arg, quote_type)) {
                    ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start, "cjsh-string");
                } else if (is_sudo_command && arg_start == token_end + 1) {
                    if (arg.rfind("./", 0) == 0) {
                        if (!std::filesystem::exists(arg) ||
                            !std::filesystem::is_regular_file(arg)) {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-unknown-command");
                        } else {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-installed");
                        }
                    } else {
                        auto cmds = g_shell->get_available_commands();
                        if (std::find(cmds.begin(), cmds.end(), arg) != cmds.end()) {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-builtin");
                        } else if (is_shell_builtin(arg)) {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-builtin");
                        } else if (basic_unix_commands_.count(arg) > 0) {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-system");
                        } else if (external_executables_.count(arg) > 0) {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-installed");
                        } else {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-unknown-command");
                        }
                    }
                } else if (is_cd_command && (arg == "~" || arg == "-")) {
                    ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                 "cjsh-path-exists");
                } else if (is_glob_pattern(arg)) {
                    ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                 "cjsh-glob-pattern");
                } else if (is_cd_command || arg[0] == '/' || arg.rfind("./", 0) == 0 ||
                           arg.rfind("../", 0) == 0 || arg.rfind("~/", 0) == 0 ||
                           arg.rfind("-/", 0) == 0 || arg.find('/') != std::string::npos) {
                    std::string path_to_check = arg;

                    if (arg.rfind("~/", 0) == 0) {
                        path_to_check = cjsh_filesystem::g_user_home_path.string() + arg.substr(1);
                    } else if (arg.rfind("-/", 0) == 0) {
                        std::string prev_dir = g_shell->get_previous_directory();
                        if (!prev_dir.empty()) {
                            path_to_check = prev_dir + arg.substr(1);
                        }
                    } else if (is_cd_command && arg[0] != '/' && arg.rfind("./", 0) != 0 &&
                               arg.rfind("../", 0) != 0 && arg.rfind("~/", 0) != 0 &&
                               arg.rfind("-/", 0) != 0) {
                        path_to_check = std::filesystem::current_path().string() + "/" + arg;
                    }

                    if (std::filesystem::exists(path_to_check)) {
                        ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                     "cjsh-path-exists");
                    } else {
                        bool is_bookmark = false;
                        if (is_cd_command && g_shell && g_shell->get_built_ins()) {
                            const auto& bookmarks =
                                g_shell->get_built_ins()->get_directory_bookmarks();
                            is_bookmark = bookmarks.find(arg) != bookmarks.end();
                        }

                        if (is_bookmark) {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-path-exists");
                        } else {
                            ic_highlight(henv, cmd_start + arg_start, arg_end - arg_start,
                                         "cjsh-path-not-exists");
                        }
                    }
                }
            }

            if (!is_variable_reference(arg) && arg.find('$') != std::string::npos) {
                highlight_quotes_and_variables(henv, input, cmd_start + arg_start,
                                               arg_end - arg_start);
            }

            arg_start = arg_end;
        }

        pos = cmd_end;
        if (pos < len) {
            if (pos + 1 < len && input[pos] == '&' && input[pos + 1] == '&') {
                ic_highlight(henv, pos, 2, "cjsh-operator");
                pos += 2;
            } else if (pos + 1 < len && input[pos] == '|' && input[pos + 1] == '|') {
                ic_highlight(henv, pos, 2, "cjsh-operator");
                pos += 2;
            } else if (pos + 1 < len && input[pos] == '>' && input[pos + 1] == '>') {
                ic_highlight(henv, pos, 2, "cjsh-operator");
                pos += 2;
            } else if (pos + 1 < len && input[pos] == '<' && input[pos + 1] == '<') {
                ic_highlight(henv, pos, 2, "cjsh-operator");
                pos += 2;
            } else if (pos + 1 < len && input[pos] == '&' && input[pos + 1] == '>') {
                ic_highlight(henv, pos, 2, "cjsh-operator");
                pos += 2;
            } else if (input[pos] == '|') {
                ic_highlight(henv, pos, 1, "cjsh-operator");
                pos += 1;
            } else if (input[pos] == ';') {
                ic_highlight(henv, pos, 1, "cjsh-operator");
                pos += 1;
            } else if (input[pos] == '>' || input[pos] == '<') {
                ic_highlight(henv, pos, 1, "cjsh-operator");
                pos += 1;
            } else if (input[pos] == '&' && (pos == len - 1 || input[pos + 1] != '&')) {
                ic_highlight(henv, pos, 1, "cjsh-operator");
                pos += 1;
            } else {
                pos += 1;
            }
        }
    }
}