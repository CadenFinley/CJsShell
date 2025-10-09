#include "variable_expander.h"

#include <unistd.h>
#include <cctype>

#include "cjsh_filesystem.h"
#include "job_control.h"
#include "parser.h"
#include "shell.h"
#include "shell_script_interpreter.h"

VariableExpander::VariableExpander(Shell* shell,
                                   const std::unordered_map<std::string, std::string>& env_vars)
    : shell(shell), env_vars(env_vars) {
}

std::string VariableExpander::get_variable_value(const std::string& var_name) {
    if ((shell != nullptr) && (shell->get_shell_script_interpreter() != nullptr)) {
        std::string result = shell->get_shell_script_interpreter()->get_variable_value(var_name);
        return result;
    }

    const char* env_val = getenv(var_name.c_str());
    std::string result = (env_val != nullptr) ? env_val : "";
    return result;
}

std::string VariableExpander::get_exported_variable_value(const std::string& var_name) {
    if (var_name == "?" || var_name == "$" || var_name == "#" || var_name == "*" ||
        var_name == "@" || var_name == "!") {
        if ((shell != nullptr) && (shell->get_shell_script_interpreter() != nullptr)) {
            return shell->get_shell_script_interpreter()->get_variable_value(var_name);
        }
    }

    if (var_name.length() == 1 && (isdigit(var_name[0]) != 0)) {
        if ((shell != nullptr) && (shell->get_shell_script_interpreter() != nullptr)) {
            return shell->get_shell_script_interpreter()->get_variable_value(var_name);
        }
    }

    const char* env_val = getenv(var_name.c_str());
    std::string result = (env_val != nullptr) ? env_val : "";
    return result;
}

std::string VariableExpander::resolve_parameter_value(const std::string& var_name) {
    if (var_name.empty()) {
        return "";
    }

    if (var_name == "?") {
        const char* status_env = getenv("?");
        return (status_env != nullptr) ? status_env : "0";
    }

    if (var_name == "$") {
        return std::to_string(getpid());
    }

    if (var_name == "#") {
        if (shell != nullptr) {
            return std::to_string(shell->get_positional_parameter_count());
        }
        return "0";
    }

    if (var_name == "*" || var_name == "@") {
        if (shell != nullptr) {
            auto params = shell->get_positional_parameters();
            std::string joined;
            for (size_t i = 0; i < params.size(); ++i) {
                if (i > 0) {
                    joined += " ";
                }
                joined += params[i];
            }
            return joined;
        }
        return "";
    }

    if (var_name == "!") {
        const char* last_bg_pid = getenv("!");
        if (last_bg_pid != nullptr) {
            return last_bg_pid;
        }

        pid_t last_pid = JobManager::instance().get_last_background_pid();
        if (last_pid > 0) {
            return std::to_string(last_pid);
        }
        return "";
    }

    if (var_name == "-") {
        std::string flags;
        auto append_flag = [&](char flag, bool enabled) {
            if (enabled && flags.find(flag) == std::string::npos) {
                flags.push_back(flag);
            }
        };

        append_flag('h', true);
        append_flag('B', true);

        if (shell != nullptr) {
            append_flag('i', shell->get_interactive_mode());
            append_flag('m', shell->is_job_control_enabled());
            append_flag('e', shell->get_shell_option("errexit"));
            append_flag('C', shell->get_shell_option("noclobber"));
            append_flag('u', shell->get_shell_option("nounset"));
            append_flag('x', shell->get_shell_option("xtrace"));
            append_flag('v', shell->get_shell_option("verbose"));
            append_flag('n', shell->get_shell_option("noexec"));
            append_flag('f', shell->get_shell_option("noglob"));
            append_flag('a', shell->get_shell_option("allexport"));
        }
        return flags;
    }

    if (!var_name.empty() && (std::isdigit(static_cast<unsigned char>(var_name[0])) != 0) &&
        var_name.length() == 1) {
        std::string value = get_variable_value(var_name);
        if (!value.empty()) {
            return value;
        }

        if (shell != nullptr) {
            auto params = shell->get_positional_parameters();
            int param_num = var_name[0] - '0';
            if (param_num > 0 && static_cast<size_t>(param_num - 1) < params.size()) {
                return params[param_num - 1];
            }
        }

        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
            return it->second;
        }
        return "";
    }

    std::string value = get_variable_value(var_name);
    if (value.empty()) {
        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
            value = it->second;
        } else if (shell != nullptr && shell->get_shell_option("nounset")) {
            std::string error_msg = var_name + ": parameter not set";
            throw std::runtime_error(error_msg);
        }
    }
    return value;
}

void VariableExpander::expand_env_vars(std::string& arg) {
    std::string result;
    result.reserve(arg.length() * 1.5);
    bool in_var = false;
    std::string var_name;
    var_name.reserve(64);

    for (size_t i = 0; i < arg.length(); ++i) {
        auto [handled, arith_result] = try_expand_arithmetic_expression(
            arg, i, [this](std::string& s) { expand_env_vars(s); },
            [this](const std::string& s) {
                if (shell != nullptr) {
                    return shell->get_parser()->evaluate_arithmetic(s);
                }
                return 0LL;
            });

        if (handled) {
            result += arith_result;
            continue;
        }

        if (arg[i] == '$' && i > 0 && arg[i - 1] == '\\') {
            if (!result.empty() && result.back() == '\\') {
                result.pop_back();
            }
            result += '$';
            continue;
        }

        if (arg[i] == '$' && i + 1 < arg.length() && arg[i + 1] == '{') {
            size_t start = i + 2;
            size_t brace_depth = 1;
            size_t end = start;

            while (end < arg.length() && brace_depth > 0) {
                if (arg[end] == '{') {
                    brace_depth++;
                } else if (arg[end] == '}') {
                    brace_depth--;
                }
                if (brace_depth > 0)
                    end++;
            }

            if (brace_depth == 0 && end < arg.length()) {
                std::string param_expr = arg.substr(start, end - start);
                std::string value;

                if (param_expr.find(':') != std::string::npos ||
                    param_expr.find('-') != std::string::npos) {
                    value = expand_parameter_with_default(
                        param_expr,
                        [this](const std::string& name) { return get_variable_value(name); },
                        [this](std::string& val) { expand_env_vars(val); });
                } else {
                    if ((shell != nullptr) && (shell->get_shell_script_interpreter() != nullptr)) {
                        try {
                            value =
                                shell->get_shell_script_interpreter()->expand_parameter_expression(
                                    param_expr);
                        } catch (...) {
                            value = get_variable_value(param_expr);
                        }
                    } else {
                        value = get_variable_value(param_expr);
                    }
                }

                result += value;
                i = end;
                continue;
            }
        }

        if (in_var) {
            if ((isalnum(arg[i]) != 0) || arg[i] == '_' ||
                (var_name.empty() && (isdigit(arg[i]) != 0)) ||
                (var_name.empty() &&
                 (arg[i] == '?' || arg[i] == '$' || arg[i] == '#' || arg[i] == '*' ||
                  arg[i] == '@' || arg[i] == '!' || arg[i] == '-'))) {
                var_name += arg[i];
            } else {
                in_var = false;
                std::string value;

                auto read_default_value = [&](size_t steps_to_advance) {
                    for (size_t step = 0; step < steps_to_advance && i < arg.length(); ++step) {
                        i++;
                    }

                    std::string default_val;
                    while (i < arg.length() && !isspace(arg[i])) {
                        default_val += arg[i];
                        i++;
                    }
                    if (i < arg.length() && isspace(arg[i])) {
                        i--;
                    }
                    expand_env_vars(default_val);
                    return default_val;
                };

                if (arg[i] == ':' && i + 1 < arg.length() && arg[i + 1] == '-') {
                    std::string env_val = get_variable_value(var_name);
                    if (!env_val.empty()) {
                        value = env_val;

                        i++;
                        i++;

                        while (i < arg.length() && (isspace(arg[i]) == 0)) {
                            i++;
                        }
                        if (i < arg.length() && (isspace(arg[i]) != 0)) {
                            i--;
                        }
                    } else {
                        value = read_default_value(2);
                    }
                } else if (arg[i] == '-' && i >= 1) {
                    std::string env_val = get_variable_value(var_name);
                    if (!env_val.empty()) {
                        value = env_val;

                        i++;
                        while (i < arg.length() && (isspace(arg[i]) == 0)) {
                            i++;
                        }
                        if (i < arg.length() && (isspace(arg[i]) != 0)) {
                            i--;
                        }
                    } else {
                        value = read_default_value(1);
                    }
                } else {
                    value = resolve_parameter_value(var_name);
                }
                result += value;

                if (arg[i] != '$' &&
                    (arg[i] != ':' || i + 1 >= arg.length() || arg[i + 1] != '-') &&
                    arg[i] != '-') {
                    result += arg[i];
                } else if (arg[i] == '$') {
                    i--;
                }
            }
        } else if (arg[i] == '$' && (i + 1 < arg.length()) &&
                   ((isalpha(arg[i + 1]) != 0) || arg[i + 1] == '_' || (isdigit(arg[i + 1]) != 0) ||
                    arg[i + 1] == '?' || arg[i + 1] == '$' || arg[i + 1] == '#' ||
                    arg[i + 1] == '*' || arg[i + 1] == '@' || arg[i + 1] == '!' ||
                    arg[i + 1] == '-')) {
            in_var = true;
            var_name.clear();
            continue;
        } else {
            result += arg[i];
        }
    }

    if (in_var) {
        result += resolve_parameter_value(var_name);
    }

    arg = result;
}

void VariableExpander::expand_env_vars_selective(std::string& arg) {
    const std::string start_marker = "\x1E__NOENV_START__\x1E";
    const std::string end_marker = "\x1E__NOENV_END__\x1E";

    std::string result;
    result.reserve(arg.length() * 1.5);

    size_t pos = 0;
    while (pos < arg.length()) {
        size_t start_pos = arg.find(start_marker, pos);

        if (start_pos == std::string::npos) {
            std::string remaining = arg.substr(pos);
            expand_env_vars(remaining);
            result += remaining;
            break;
        }

        std::string before_marker = arg.substr(pos, start_pos - pos);
        expand_env_vars(before_marker);
        result += before_marker;

        size_t end_pos = arg.find(end_marker, start_pos + start_marker.length());
        if (end_pos == std::string::npos) {
            result += arg.substr(start_pos);
            break;
        }

        size_t content_start = start_pos + start_marker.length();
        size_t content_length = end_pos - content_start;
        result += arg.substr(content_start, content_length);

        pos = end_pos + end_marker.length();
    }

    arg = result;
}

void VariableExpander::expand_exported_env_vars_only(std::string& arg) {
    std::string result;
    result.reserve(arg.length() * 1.5);
    bool in_var = false;
    std::string var_name;
    var_name.reserve(64);

    for (size_t i = 0; i < arg.length(); ++i) {
        auto [handled, arith_result] = try_expand_arithmetic_expression(
            arg, i, [this](std::string& s) { expand_exported_env_vars_only(s); },
            [this](const std::string& s) {
                if (shell != nullptr) {
                    return shell->get_parser()->evaluate_arithmetic(s);
                }
                return 0LL;
            });

        if (handled) {
            result += arith_result.empty() ? "0" : arith_result;
            continue;
        }

        if (arg[i] == '$' && !in_var) {
            in_var = true;
            var_name.clear();
        } else if (in_var) {
            if (arg[i] == '{') {
                size_t brace_start = i + 1;
                size_t brace_end = brace_start;
                int brace_depth = 1;

                while (brace_end < arg.length() && brace_depth > 0) {
                    if (arg[brace_end] == '{') {
                        brace_depth++;
                    } else if (arg[brace_end] == '}') {
                        brace_depth--;
                    }
                    if (brace_depth > 0)
                        brace_end++;
                }

                if (brace_depth == 0) {
                    std::string param_expr = arg.substr(brace_start, brace_end - brace_start);
                    std::string value;

                    if (param_expr.find(":-") != std::string::npos ||
                        param_expr.find(":=") != std::string::npos) {
                        value = expand_parameter_with_default(
                            param_expr,
                            [this](const std::string& name) {
                                return get_exported_variable_value(name);
                            },
                            [this](std::string& val) { expand_exported_env_vars_only(val); });
                    } else {
                        value = get_exported_variable_value(param_expr);
                    }

                    result += value;
                    i = brace_end;
                    in_var = false;
                } else {
                    result += arg[i];
                }
            } else if ((std::isalnum(arg[i]) != 0) || arg[i] == '_' || arg[i] == '?' ||
                       arg[i] == '$' || arg[i] == '#' || arg[i] == '*' || arg[i] == '@' ||
                       arg[i] == '!' || arg[i] == '-' || (std::isdigit(arg[i]) != 0)) {
                var_name += arg[i];
            } else {
                std::string value = get_exported_variable_value(var_name);
                result += value;

                if (arg[i] != '$' &&
                    (arg[i] != ':' || i + 1 >= arg.length() || arg[i + 1] != '-') &&
                    arg[i] != '-') {
                    result += arg[i];
                } else if (arg[i] == '$') {
                    i--;
                }
                in_var = false;
                var_name.clear();
            }
        } else {
            result += arg[i];
        }
    }

    if (in_var) {
        std::string value = get_exported_variable_value(var_name);
        result += value;
    }

    arg = result;
}

void VariableExpander::expand_command_substitutions_in_string(std::string& text) {
    if (text.find('$') == std::string::npos && text.find('`') == std::string::npos) {
        return;
    }

    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size();) {
        if (text[i] == '$' && i + 1 < text.size() && text[i + 1] == '(') {
            size_t pos = i + 2;
            int depth = 1;
            bool in_single = false;
            bool in_double = false;
            while (pos < text.size() && depth > 0) {
                char ch = text[pos];
                if (ch == '\\') {
                    pos += 2;
                    continue;
                }
                if (ch == '\'' && !in_double) {
                    in_single = !in_single;
                } else if (ch == '"' && !in_single) {
                    in_double = !in_double;
                } else if (!in_single) {
                    if (ch == '(') {
                        depth++;
                    } else if (ch == ')') {
                        depth--;
                        if (depth == 0) {
                            break;
                        }
                    }
                }
                pos++;
            }

            if (depth == 0 && pos < text.size()) {
                std::string command = text.substr(i + 2, pos - (i + 2));
                auto output = cjsh_filesystem::read_command_output(command);
                if (output.is_ok()) {
                    std::string value = output.value();
                    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                        value.pop_back();
                    }
                    result += value;
                    i = pos + 1;
                    continue;
                }
            }
        } else if (text[i] == '`') {
            size_t pos = i + 1;
            while (pos < text.size()) {
                if (text[pos] == '\\' && pos + 1 < text.size()) {
                    pos += 2;
                    continue;
                }
                if (text[pos] == '`') {
                    break;
                }
                pos++;
            }
            if (pos < text.size() && text[pos] == '`') {
                std::string command = text.substr(i + 1, pos - (i + 1));
                std::string cleaned;
                cleaned.reserve(command.size());
                for (size_t k = 0; k < command.size(); ++k) {
                    if (command[k] == '\\' && k + 1 < command.size() && command[k + 1] == '`') {
                        cleaned.push_back('`');
                        ++k;
                    } else {
                        cleaned.push_back(command[k]);
                    }
                }
                auto output = cjsh_filesystem::read_command_output(cleaned);
                if (output.is_ok()) {
                    std::string value = output.value();
                    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                        value.pop_back();
                    }
                    result += value;
                    i = pos + 1;
                    continue;
                }
            }
        }

        result += text[i];
        ++i;
    }

    text.swap(result);
}

void VariableExpander::expand_command_paths_with_home(Command& cmd, const std::string& home) {
    auto expand_path = [&](std::string& path) {
        if (!path.empty() && path.front() == '~') {
            path = home + path.substr(1);
        }
    };

    expand_path(cmd.input_file);
    expand_path(cmd.output_file);
    expand_path(cmd.append_file);
    expand_path(cmd.stderr_file);
    expand_path(cmd.both_output_file);

    for (auto& fd_redir : cmd.fd_redirections) {
        expand_path(fd_redir.second);
    }
}

void VariableExpander::set_use_exported_vars_only(bool value) {
    use_exported_vars_only = value;
}

bool VariableExpander::get_use_exported_vars_only() const {
    return use_exported_vars_only;
}
