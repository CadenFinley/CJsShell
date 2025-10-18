#include "command_substitution_evaluator.h"

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "cjsh_filesystem.h"

namespace {

std::pair<std::string, int> execute_command_for_substitution(
    const std::string& command, const std::function<int(const std::string&)>& executor) {
    char tmpl[] = "/tmp/cjsh_subst_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0)
        close(fd);
    std::string path = tmpl;

    int saved_stdout = dup(STDOUT_FILENO);

    auto temp_file_result = cjsh_filesystem::safe_fopen(path, "w");
    if (temp_file_result.is_error()) {
        int pipefd[2];
        if (pipe(pipefd) != 0) {
            return {"", 1};
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            int exit_code = executor(command);
            exit(exit_code);
        } else if (pid > 0) {
            close(pipefd[1]);
            std::string result;
            char buf[4096];
            ssize_t n = 0;
            while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
                result.append(buf, n);
            }
            close(pipefd[0]);

            int status = 0;
            waitpid(pid, &status, 0);

            int exit_code = 0;
            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                exit_code = 128 + WTERMSIG(status);
            } else {
                exit_code = status;
            }

            while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                result.pop_back();

            return {result, exit_code};
        } else {
            close(pipefd[0]);
            close(pipefd[1]);
            return {"", 1};
        }
    }

    FILE* temp_file = temp_file_result.value();
    int temp_fd = fileno(temp_file);
    auto dup_result = cjsh_filesystem::safe_dup2(temp_fd, STDOUT_FILENO);
    if (dup_result.is_error()) {
        cjsh_filesystem::safe_fclose(temp_file);
        cjsh_filesystem::safe_close(saved_stdout);
        return {"", 1};
    }

    int exit_code = executor(command);

    (void)fflush(stdout);
    cjsh_filesystem::safe_fclose(temp_file);
    auto restore_result = cjsh_filesystem::safe_dup2(saved_stdout, STDOUT_FILENO);
    cjsh_filesystem::safe_close(saved_stdout);

    auto content_result = cjsh_filesystem::read_file_content(path);
    cjsh_filesystem::cleanup_temp_file(path);

    if (content_result.is_error()) {
        return {"", exit_code};
    }

    std::string out = content_result.value();

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();

    return {out, exit_code};
}

}  // namespace

CommandSubstitutionEvaluator::CommandSubstitutionEvaluator(CommandExecutor executor)
    : command_executor_(std::move(executor)) {
}

std::pair<std::string, int> CommandSubstitutionEvaluator::capture_command_output(
    const std::string& command) {
    return command_executor_(command);
}

CommandSubstitutionEvaluator::CommandExecutor CommandSubstitutionEvaluator::create_command_executor(
    const std::function<int(const std::string&)>& executor) {
    return [executor](const std::string& command) -> std::pair<std::string, int> {
        return execute_command_for_substitution(command, executor);
    };
}

bool CommandSubstitutionEvaluator::find_matching_delimiter(const std::string& text, size_t start,
                                                           char open_c, char close_c,
                                                           size_t& end_out) {
    int depth = 1;
    bool local_in_q = false;
    char local_q = '\0';

    for (size_t j = start; j < text.size(); ++j) {
        char d = text[j];
        if ((d == '"' || d == '\'') && (j == start || text[j - 1] != '\\')) {
            if (!local_in_q) {
                local_in_q = true;
                local_q = d;
            } else if (local_q == d) {
                local_in_q = false;
                local_q = '\0';
            }
        } else if (!local_in_q) {
            if (d == open_c) {
                depth++;
            } else if (d == close_c) {
                depth--;
                if (depth == 0) {
                    end_out = j;
                    return true;
                }
            }
        }
    }
    return false;
}

std::string CommandSubstitutionEvaluator::escape_for_double_quotes(const std::string& content) {
    std::string result;
    result.reserve(content.size() + (content.size() / 10) + 1);

    for (char c : content) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') {
            result += '\\';
        }
        result += c;
    }

    return result;
}

bool CommandSubstitutionEvaluator::try_handle_arithmetic_expansion(const std::string& input,
                                                                   size_t& i,
                                                                   std::string& output_text) {
    char c = input[i];
    if (c != '$' || i + 2 >= input.size() || input[i + 1] != '(' || input[i + 2] != '(') {
        return false;
    }

    size_t arith_end = 0;
    if (!find_matching_delimiter(input, i + 3, '(', ')', arith_end)) {
        return false;
    }

    if (arith_end + 1 >= input.size() || input[arith_end + 1] != ')') {
        return false;
    }

    std::string inner = input.substr(i + 3, arith_end - (i + 3));
    if (inner.find(';') != std::string::npos) {
        return false;
    }

    for (size_t k = i; k <= arith_end + 1; ++k) {
        output_text += input[k];
    }
    i = arith_end + 1;
    return true;
}

bool CommandSubstitutionEvaluator::try_handle_command_substitution(const std::string& input,
                                                                   size_t& i,
                                                                   ExpansionResult& result,
                                                                   bool in_double_quotes) {
    char c = input[i];
    if (c != '$' || i + 1 >= input.size() || input[i + 1] != '(') {
        return false;
    }

    size_t cmd_end = 0;
    if (!find_matching_delimiter(input, i + 2, '(', ')', cmd_end)) {
        return false;
    }

    std::string cmd_content = input.substr(i + 2, cmd_end - i - 2);
    auto [cmd_output, exit_code] = capture_command_output(cmd_content);
    result.outputs.push_back(cmd_output);
    result.exit_codes.push_back(exit_code);
    append_substitution_result(cmd_output, in_double_quotes, result.text);
    i = cmd_end;
    return true;
}

bool CommandSubstitutionEvaluator::try_handle_backtick_substitution(const std::string& input,
                                                                    size_t& i,
                                                                    ExpansionResult& result,
                                                                    bool in_double_quotes) {
    if (input[i] != '`') {
        return false;
    }

    size_t backtick_end = find_closing_backtick(input, i + 1);
    if (backtick_end == std::string::npos) {
        return false;
    }

    std::string cmd_content = input.substr(i + 1, backtick_end - i - 1);
    auto [cmd_output, exit_code] = capture_command_output(cmd_content);
    result.outputs.push_back(cmd_output);
    result.exit_codes.push_back(exit_code);
    append_substitution_result(cmd_output, in_double_quotes, result.text);
    i = backtick_end;
    return true;
}

bool CommandSubstitutionEvaluator::try_handle_parameter_expansion(const std::string& input,
                                                                  size_t& i,
                                                                  std::string& output_text) {
    char c = input[i];
    if (c != '$' || i + 1 >= input.size() || input[i + 1] != '{') {
        return false;
    }

    size_t brace_end = 0;
    if (!find_matching_delimiter(input, i + 2, '{', '}', brace_end)) {
        return false;
    }

    for (size_t k = i; k <= brace_end; ++k) {
        output_text += input[k];
    }
    i = brace_end;
    return true;
}

size_t CommandSubstitutionEvaluator::find_closing_backtick(const std::string& input, size_t start) {
    bool bt_escaped = false;
    for (size_t pos = start; pos < input.size(); ++pos) {
        if (bt_escaped) {
            bt_escaped = false;
            continue;
        }
        if (input[pos] == '\\') {
            bt_escaped = true;
            continue;
        }
        if (input[pos] == '`') {
            return pos;
        }
    }
    return std::string::npos;
}

void CommandSubstitutionEvaluator::append_substitution_result(const std::string& content,
                                                              bool in_double_quotes,
                                                              std::string& output) {
    if (in_double_quotes) {
        std::string esc = escape_for_double_quotes(content);
        output += NOENV_START;
        output += esc;
        output += NOENV_END;
    } else {
        output += SUBST_LITERAL_START;
        output += content;
        output += SUBST_LITERAL_END;
    }
}

bool CommandSubstitutionEvaluator::handle_escape_sequence(char c, bool& escaped,
                                                          std::string& output) {
    if (escaped) {
        output += '\\';
        output += c;
        escaped = false;
        return true;
    }
    return false;
}

bool CommandSubstitutionEvaluator::handle_quote_toggle(char c, bool, bool& in_quotes,
                                                       char& quote_char, std::string& output) {
    if ((c == '"' || c == '\'') && !in_quotes) {
        in_quotes = true;
        quote_char = c;
        output += c;
        return true;
    }
    if (in_quotes && c == quote_char) {
        in_quotes = false;
        quote_char = '\0';
        output += c;
        return true;
    }
    return false;
}

CommandSubstitutionEvaluator::ExpansionResult CommandSubstitutionEvaluator::expand_substitutions(
    const std::string& input) {
    ExpansionResult result;
    result.text.reserve(input.size());

    bool in_quotes = false;
    char q = '\0';
    bool escaped = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (handle_escape_sequence(c, escaped, result.text)) {
            continue;
        }

        if (c == '\\' && (!in_quotes || q != '\'')) {
            escaped = true;
            continue;
        }

        if (handle_quote_toggle(c, q == '\'', in_quotes, q, result.text)) {
            continue;
        }

        bool can_substitute = !in_quotes || q == '"';
        if (can_substitute) {
            bool in_double_quotes = in_quotes && q == '"';

            if (try_handle_arithmetic_expansion(input, i, result.text)) {
                continue;
            }

            if (try_handle_command_substitution(input, i, result, in_double_quotes)) {
                continue;
            }

            if (try_handle_backtick_substitution(input, i, result, in_double_quotes)) {
                continue;
            }

            if (try_handle_parameter_expansion(input, i, result.text)) {
                continue;
            }
        }

        result.text += c;
    }

    return result;
}

std::optional<size_t> CommandSubstitutionEvaluator::find_matching_paren(const std::string& text,
                                                                        size_t start_index) {
    int depth = 1;
    for (size_t i = start_index; i < text.size(); ++i) {
        if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')') {
            depth--;
            if (depth == 0)
                return i;
        }
    }
    return std::nullopt;
}
