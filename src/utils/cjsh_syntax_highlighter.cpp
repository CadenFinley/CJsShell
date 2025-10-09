#include "cjsh_syntax_highlighter.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "builtin.h"
#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "shell.h"

const std::unordered_set<std::string> comparison_operators = {
    "=",   "==",  "!=",  "<",   "<=",  ">",   ">=",  "-eq",
    "-ne", "-gt", "-ge", "-lt", "-le", "-ef", "-nt", "-ot"};

const std::unordered_set<std::string> SyntaxHighlighter::basic_unix_commands_ = {
    "cat",  "mv",    "cp",    "rm", "mkdir", "rmdir", "touch",  "grep",
    "find", "chmod", "chown", "ps", "man",   "which", "whereis"};
std::unordered_set<std::string> SyntaxHighlighter::external_executables_;
std::shared_mutex SyntaxHighlighter::external_cache_mutex_;
const std::unordered_set<std::string> SyntaxHighlighter::command_operators_ = {"&&", "||", "|",
                                                                               ";"};

const std::unordered_set<std::string> SyntaxHighlighter::shell_keywords_ = {
    "if",     "then",  "else", "elif", "fi",   "case",     "in",     "esac",
    "while",  "until", "for",  "do",   "done", "function", "select", "time",
    "coproc", "{",     "}",    "[[",   "]]",   "(",        ")",      ":"};

const std::unordered_set<std::string> SyntaxHighlighter::shell_built_ins_ = {
    "echo",    "printf", "pwd",      "cd",          "ls",        "alias",    "export", "unalias",
    "unset",   "set",    "shift",    "break",       "continue",  "return",   "source", ".",
    "theme",   "help",   "approot",  "version",     "uninstall", "eval",     "syntax", "history",
    "exit",    "quit",   "terminal", "prompt_test", "test",      "[",        "exec",   "trap",
    "jobs",    "fg",     "bg",       "wait",        "kill",      "readonly", "read",   "umask",
    "getopts", "times",  "type",     "hash"};

void SyntaxHighlighter::initialize_syntax_highlighting() {
    if (config::syntax_highlighting_enabled) {
        SyntaxHighlighter::initialize();
        ic_set_default_highlighter(SyntaxHighlighter::highlight, nullptr);
        ic_enable_highlight(true);
    } else {
        ic_set_default_highlighter(nullptr, nullptr);
        ic_enable_highlight(false);
    }
}

void SyntaxHighlighter::initialize() {
    std::unique_lock<std::shared_mutex> lock(external_cache_mutex_);
    external_executables_.clear();
    for (const auto& e : cjsh_filesystem::read_cached_executables()) {
        external_executables_.insert(e.filename().string());
    }
}

void SyntaxHighlighter::refresh_executables_cache() {
    std::unique_lock<std::shared_mutex> lock(external_cache_mutex_);
    external_executables_.clear();
    for (const auto& e : cjsh_filesystem::read_cached_executables()) {
        external_executables_.insert(e.filename().string());
    }
}

bool SyntaxHighlighter::is_external_command(const std::string& token) {
    std::shared_lock<std::shared_mutex> lock(external_cache_mutex_);
    return external_executables_.count(token) > 0;
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

        if (!var_name.empty() && ((std::isalpha(var_name[0]) != 0) || var_name[0] == '_')) {
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

bool SyntaxHighlighter::is_option(const std::string& token) {
    if (token.size() < 2 || token[0] != '-') {
        return false;
    }

    if (token == "-" || token == "--") {
        return false;
    }

    if (token.rfind("--", 0) == 0) {
        return token.size() > 2;
    }

    bool numeric_like = true;
    for (size_t idx = 1; idx < token.size(); ++idx) {
        unsigned char uch = static_cast<unsigned char>(token[idx]);
        if ((std::isdigit(uch) == 0) && token[idx] != '.') {
            numeric_like = false;
            break;
        }
    }

    return !numeric_like;
}

bool SyntaxHighlighter::is_numeric_literal(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    size_t start = 0;
    if (token[start] == '+' || token[start] == '-') {
        start++;
    }

    if (start >= token.size()) {
        return false;
    }

    if (token.size() - start > 2 && token[start] == '0' &&
        (token[start + 1] == 'x' || token[start + 1] == 'X')) {
        if (start + 2 >= token.size()) {
            return false;
        }
        for (size_t idx = start + 2; idx < token.size(); ++idx) {
            unsigned char uch = static_cast<unsigned char>(token[idx]);
            if ((std::isdigit(uch) == 0) && (uch < 'a' || uch > 'f') && (uch < 'A' || uch > 'F')) {
                return false;
            }
        }
        return true;
    }

    bool saw_digit = false;
    bool saw_dot = false;
    bool saw_exponent = false;

    for (size_t i = start; i < token.size(); ++i) {
        unsigned char uch = static_cast<unsigned char>(token[i]);

        if ((std::isdigit(uch) != 0)) {
            saw_digit = true;
            continue;
        }

        if (token[i] == '.' && !saw_dot && !saw_exponent) {
            saw_dot = true;
            continue;
        }

        if ((token[i] == 'e' || token[i] == 'E') && !saw_exponent && saw_digit) {
            saw_exponent = true;
            saw_digit = false;
            if (i + 1 < token.size() && (token[i + 1] == '+' || token[i + 1] == '-')) {
                ++i;
            }
            continue;
        }

        return false;
    }

    return saw_digit;
}

bool SyntaxHighlighter::is_function_definition(const std::string& input, size_t& func_name_start,
                                               size_t& func_name_end) {
    func_name_start = 0;
    func_name_end = 0;

    const std::string& trimmed = input;

    size_t first_non_space = trimmed.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return false;
    }

    if (trimmed.substr(first_non_space, 8) == "function") {
        size_t name_start = first_non_space + 8;

        while (name_start < trimmed.length() && (std::isspace(trimmed[name_start]) != 0)) {
            name_start++;
        }

        if (name_start >= trimmed.length()) {
            return false;
        }

        size_t name_end = name_start;
        while (name_end < trimmed.length() && (std::isspace(trimmed[name_end]) == 0) &&
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

        while (name_end > name_start &&
               (std::isspace(static_cast<unsigned char>(trimmed[name_end - 1])) != 0)) {
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

void SyntaxHighlighter::highlight_variable_assignment(ic_highlight_env_t* henv, const char* input,
                                                      size_t absolute_start,
                                                      const std::string& token) {
    size_t eq_pos = token.find('=');
    if (eq_pos == std::string::npos) {
        highlight_quotes_and_variables(henv, input, absolute_start, token.length());
        return;
    }

    if (eq_pos == 0) {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(token.length()),
                     "cjsh-variable");
        return;
    }

    ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(eq_pos),
                 "cjsh-variable");
    ic_highlight(henv, static_cast<long>(absolute_start + eq_pos), 1L, "cjsh-operator");

    if (eq_pos + 1 >= token.length()) {
        return;
    }

    std::string value = token.substr(eq_pos + 1);
    highlight_assignment_value(henv, input, absolute_start + eq_pos + 1, value);
}

void SyntaxHighlighter::highlight_assignment_value(ic_highlight_env_t* henv, const char* input,
                                                   size_t absolute_start,
                                                   const std::string& value) {
    if (value.empty()) {
        return;
    }

    ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                 "cjsh-assignment-value");

    char quote_type = 0;
    if (is_quoted_string(value, quote_type)) {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                     "cjsh-string");
        return;
    }

    if (is_numeric_literal(value)) {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                     "cjsh-number");
        return;
    }

    if (!value.empty() && value[0] == '$') {
        ic_highlight(henv, static_cast<long>(absolute_start), static_cast<long>(value.length()),
                     "cjsh-variable");
        highlight_quotes_and_variables(henv, input, absolute_start, value.length());
        return;
    }

    if (value.find('$') != std::string::npos || value.find('`') != std::string::npos ||
        value.find("$(") != std::string::npos) {
        highlight_quotes_and_variables(henv, input, absolute_start, value.length());
    }
}

void SyntaxHighlighter::highlight_quotes_and_variables(ic_highlight_env_t* henv, const char* input,
                                                       size_t start,
                                                       size_t length) {  // NOLINT
    if (length == 0) {
        return;
    }

    const size_t end = start + length;

    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;
    size_t single_quote_start = 0;
    size_t double_quote_start = 0;

    for (size_t i = start; i < end; ++i) {
        char c = input[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\' && !in_single_quote) {
            escaped = true;
            continue;
        }

        if (!in_single_quote && c == '$' && i + 1 < end && input[i + 1] == '(') {
            bool is_arithmetic = (i + 2 < end && input[i + 2] == '(');
            size_t j = i + 2;
            int depth = 1;
            bool inner_single = false;
            bool inner_double = false;
            bool inner_escaped = false;
            while (j < end) {
                char inner = input[j];
                if (inner_escaped) {
                    inner_escaped = false;
                } else if (inner == '\\' && !inner_single) {
                    inner_escaped = true;
                } else if (inner == '\'' && !inner_double) {
                    inner_single = !inner_single;
                } else if (inner == '"' && !inner_single) {
                    inner_double = !inner_double;
                } else if (!inner_single) {
                    if (inner == '(') {
                        depth++;
                    } else if (inner == ')') {
                        depth--;
                        if (depth == 0) {
                            break;
                        }
                    }
                }
                ++j;
            }

            if (depth == 0 && j < end) {
                size_t highlight_len = (j + 1) - i;
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(highlight_len),
                             is_arithmetic ? "cjsh-arithmetic" : "cjsh-command-substitution");
                i = j;
                continue;
            }
        }

        if (!in_single_quote && c == '`') {
            size_t j = i + 1;
            bool inner_escaped = false;
            while (j < end) {
                char inner = input[j];
                if (inner_escaped) {
                    inner_escaped = false;
                } else if (inner == '\\') {
                    inner_escaped = true;
                } else if (inner == '`') {
                    break;
                }
                ++j;
            }

            if (j < end) {
                size_t highlight_len = (j + 1) - i;
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(highlight_len),
                             "cjsh-command-substitution");
                i = j;
                continue;
            }
        }

        if (!in_single_quote && !in_double_quote && c == '(' && i + 1 < end &&
            input[i + 1] == '(') {
            size_t j = i + 2;
            int depth = 2;
            bool inner_single = false;
            bool inner_double = false;
            bool inner_escaped = false;
            while (j < end) {
                char inner = input[j];
                if (inner_escaped) {
                    inner_escaped = false;
                } else if (inner == '\\' && !inner_single) {
                    inner_escaped = true;
                } else if (inner == '\'' && !inner_double) {
                    inner_single = !inner_single;
                } else if (inner == '"' && !inner_single) {
                    inner_double = !inner_double;
                } else if (!inner_single) {
                    if (inner == '(') {
                        depth++;
                    } else if (inner == ')') {
                        depth--;
                        if (depth == 0) {
                            break;
                        }
                    }
                }
                ++j;
            }

            if (depth == 0 && j < end) {
                size_t highlight_len = (j + 1) - i;
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(highlight_len),
                             "cjsh-arithmetic");
                i = j;
                continue;
            }
        }

        if (c == '\'' && !in_double_quote) {
            if (!in_single_quote) {
                in_single_quote = true;
                single_quote_start = i;
            } else {
                in_single_quote = false;
                ic_highlight(henv, static_cast<long>(single_quote_start),
                             static_cast<long>(i - single_quote_start + 1), "cjsh-string");
            }
            continue;
        }

        if (c == '"' && !in_single_quote) {
            if (!in_double_quote) {
                in_double_quote = true;
                double_quote_start = i;
            } else {
                in_double_quote = false;
                ic_highlight(henv, static_cast<long>(double_quote_start),
                             static_cast<long>(i - double_quote_start + 1), "cjsh-string");
            }
            continue;
        }

        if (c == '$' && !in_single_quote) {
            size_t var_start = i;
            size_t var_end = i + 1;

            if (var_end < end && input[var_end] == '{') {
                ++var_end;
                while (var_end < end && input[var_end] != '}') {
                    ++var_end;
                }
                if (var_end < end) {
                    ++var_end;
                }
            } else {
                while (var_end < end) {
                    char vc = input[var_end];
                    if ((std::isalnum(static_cast<unsigned char>(vc)) != 0) || vc == '_' ||
                        (var_end == var_start + 1 &&
                         (std::isdigit(static_cast<unsigned char>(vc)) != 0))) {
                        ++var_end;
                    } else {
                        break;
                    }
                }
            }

            if (var_end > var_start + 1) {
                ic_highlight(henv, static_cast<long>(var_start),
                             static_cast<long>(var_end - var_start), "cjsh-variable");
                i = var_end - 1;
            }
        }
    }
}

void SyntaxHighlighter::highlight(ic_highlight_env_t* henv, const char* input,
                                  void*) {  // NOLINT
    size_t len = std::strlen(input);
    if (len == 0)
        return;

    std::string sanitized_input(input, len);

    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    size_t i = 0;
    while (i < len) {
        char c = input[i];

        if (escaped) {
            escaped = false;
            ++i;
            continue;
        }

        if (c == '\\' && (!in_quotes || quote_char != '\'')) {
            escaped = true;
            ++i;
            continue;
        }

        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
            ++i;
            continue;
        }

        if (c == quote_char && in_quotes) {
            in_quotes = false;
            quote_char = '\0';
            ++i;
            continue;
        }

        if (!in_quotes && c == '#') {
            size_t comment_end = i;
            while (comment_end < len && input[comment_end] != '\n' && input[comment_end] != '\r') {
                sanitized_input[comment_end] = ' ';
                comment_end++;
            }
            if (comment_end > i) {
                ic_highlight(henv, static_cast<long>(i), static_cast<long>(comment_end - i),
                             "cjsh-comment");
            }
            i = comment_end;
            continue;
        }

        ++i;
    }

    const char* analysis = sanitized_input.c_str();

    size_t func_name_start = 0;
    size_t func_name_end = 0;
    std::string input_str = sanitized_input;
    if (is_function_definition(input_str, func_name_start, func_name_end)) {
        ic_highlight(henv, static_cast<long>(func_name_start),
                     static_cast<long>(func_name_end - func_name_start),
                     "cjsh-function-definition");

        size_t paren_pos = input_str.find("()", func_name_end);
        if (paren_pos != std::string::npos && paren_pos < len) {
            ic_highlight(henv, static_cast<long>(paren_pos), 2L, "cjsh-function-definition");
        }

        size_t brace_pos = input_str.find('{');
        if (brace_pos != std::string::npos && brace_pos < len) {
            ic_highlight(henv, static_cast<long>(brace_pos), 1L, "cjsh-operator");
        }
        return;
    }

    size_t pos = 0;
    while (pos < len) {
        size_t cmd_end = pos;
        while (cmd_end < len) {
            if ((cmd_end + 1 < len && analysis[cmd_end] == '&' && analysis[cmd_end + 1] == '&') ||
                (cmd_end + 1 < len && analysis[cmd_end] == '|' && analysis[cmd_end + 1] == '|') ||
                analysis[cmd_end] == '|' || analysis[cmd_end] == ';' || analysis[cmd_end] == '\n' ||
                analysis[cmd_end] == '\r') {
                break;
            }
            cmd_end++;
        }

        size_t cmd_start = pos;
        while (cmd_start < cmd_end && (std::isspace((unsigned char)analysis[cmd_start]) != 0)) {
            cmd_start++;
        }

        std::string cmd_str(analysis + cmd_start, cmd_end - cmd_start);

        size_t token_end = 0;
        while (token_end < cmd_str.length() &&
               (std::isspace((unsigned char)cmd_str[token_end]) == 0)) {
            token_end++;
        }

        std::string token = token_end > 0 ? cmd_str.substr(0, token_end) : "";

        bool is_sudo_command = (token == "sudo");

        if (!token.empty()) {
            bool handled_first_token = false;

            if (is_variable_reference(token)) {
                highlight_variable_assignment(henv, input, cmd_start, token);
                handled_first_token = true;
            }

            if (!handled_first_token && (token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0 ||
                                         token.rfind("~/", 0) == 0 || token.rfind("-/", 0) == 0 ||
                                         token[0] == '/' || token.find('/') != std::string::npos)) {
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
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-installed");
                } else {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-unknown-command");
                }
                handled_first_token = true;
            }

            if (!handled_first_token && is_shell_keyword(token)) {
                ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                             "cjsh-keyword");
                handled_first_token = true;
            } else if (!handled_first_token && is_shell_builtin(token)) {
                ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                             "cjsh-builtin");
                handled_first_token = true;
            }

            if (!handled_first_token) {
                auto cmds = g_shell->get_available_commands();
                if (cmds.find(token) != cmds.end()) {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-builtin");
                } else if (basic_unix_commands_.count(token) > 0) {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-system");
                } else if (is_external_command(token)) {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-installed");
                } else {
                    ic_highlight(henv, static_cast<long>(cmd_start), static_cast<long>(token_end),
                                 "cjsh-unknown-command");
                }
            }
        }

        bool is_cd_command = (token == "cd");
        size_t arg_start = token_end;

        while (arg_start < cmd_str.length()) {
            while (arg_start < cmd_str.length() &&
                   (std::isspace((unsigned char)cmd_str[arg_start]) != 0)) {
                arg_start++;
            }
            if (arg_start >= cmd_str.length())
                break;

            size_t arg_end = arg_start;
            while (arg_end < cmd_str.length() &&
                   (std::isspace((unsigned char)cmd_str[arg_end]) == 0)) {
                arg_end++;
            }

            std::string arg = cmd_str.substr(arg_start, arg_end - arg_start);

            if (is_redirection_operator(arg) || comparison_operators.count(arg) > 0) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-operator");
            }

            else if (is_variable_reference(arg)) {
                highlight_variable_assignment(henv, input, cmd_start + arg_start, arg);
            }

            else if (arg == "((" || arg == "))") {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-arithmetic");
            }

            else if (is_shell_keyword(arg)) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-keyword");
            }

            else if (is_option(arg)) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-option");
            }

            else if (is_numeric_literal(arg)) {
                ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                             static_cast<long>(arg_end - arg_start), "cjsh-number");
            }

            else {
                char quote_type = 0;
                if (is_quoted_string(arg, quote_type)) {
                    ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                 static_cast<long>(arg_end - arg_start), "cjsh-string");
                } else if (is_sudo_command && arg_start == token_end + 1) {
                    if (arg.rfind("./", 0) == 0) {
                        if (!std::filesystem::exists(arg) ||
                            !std::filesystem::is_regular_file(arg)) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-unknown-command");
                        } else {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-installed");
                        }
                    } else {
                        auto cmds = g_shell->get_available_commands();
                        if (cmds.find(arg) != cmds.end() || is_shell_builtin(arg)) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-builtin");
                        } else if (basic_unix_commands_.count(arg) > 0) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-system");
                        } else if (is_external_command(arg)) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start), "cjsh-installed");
                        } else {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-unknown-command");
                        }
                    }
                } else if (is_cd_command && (arg == "~" || arg == "-")) {
                    ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                 static_cast<long>(arg_end - arg_start), "cjsh-path-exists");
                } else if (is_glob_pattern(arg)) {
                    ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                 static_cast<long>(arg_end - arg_start), "cjsh-glob-pattern");
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
                        ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                     static_cast<long>(arg_end - arg_start), "cjsh-path-exists");
                    } else {
                        bool is_bookmark = false;
                        if (is_cd_command && g_shell && (g_shell->get_built_ins() != nullptr)) {
                            const auto& bookmarks =
                                g_shell->get_built_ins()->get_directory_bookmarks();
                            is_bookmark = bookmarks.find(arg) != bookmarks.end();
                        }

                        if (is_bookmark) {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-path-exists");
                        } else {
                            ic_highlight(henv, static_cast<long>(cmd_start + arg_start),
                                         static_cast<long>(arg_end - arg_start),
                                         "cjsh-path-not-exists");
                        }
                    }
                }
            }

            if (!is_variable_reference(arg) &&
                (arg.find('$') != std::string::npos || arg.find('`') != std::string::npos)) {
                highlight_quotes_and_variables(henv, input, cmd_start + arg_start,
                                               arg_end - arg_start);
            }

            arg_start = arg_end;
        }

        pos = cmd_end;
        if (pos < len) {
            if (pos + 1 < len && ((analysis[pos] == '&' && analysis[pos + 1] == '&') ||
                                  (analysis[pos] == '|' && analysis[pos + 1] == '|') ||
                                  (analysis[pos] == '>' && analysis[pos + 1] == '>') ||
                                  (analysis[pos] == '<' && analysis[pos + 1] == '<') ||
                                  (analysis[pos] == '&' && analysis[pos + 1] == '>'))) {
                ic_highlight(henv, static_cast<long>(pos), 2, "cjsh-operator");
                pos += 2;
            } else if (analysis[pos] == '|' || analysis[pos] == ';' || analysis[pos] == '>' ||
                       analysis[pos] == '<' ||
                       (analysis[pos] == '&' && (pos == len - 1 || analysis[pos + 1] != '&'))) {
                ic_highlight(henv, static_cast<long>(pos), 1, "cjsh-operator");
                pos += 1;
            } else {
                if (analysis[pos] == '\r' && pos + 1 < len && analysis[pos + 1] == '\n') {
                    pos += 2;
                } else {
                    pos += 1;
                }
            }
        }
    }
}