#pragma once

#include <functional>
#include <string>
#include <unordered_map>

class Shell;
struct Command;

class VariableExpander {
   public:
    VariableExpander(Shell* shell, const std::unordered_map<std::string, std::string>& env_vars);

    void expand_env_vars(std::string& arg);
    void expand_env_vars_selective(std::string& arg);
    void expand_exported_env_vars_only(std::string& arg);

    std::string get_variable_value(const std::string& var_name);
    std::string get_exported_variable_value(const std::string& var_name);
    std::string resolve_parameter_value(const std::string& var_name);

    void expand_command_substitutions_in_string(std::string& text);
    void expand_command_paths_with_home(Command& cmd, const std::string& home);
    void expand_command_redirection_paths(Command& cmd);

    void set_use_exported_vars_only(bool value);
    bool get_use_exported_vars_only() const;

    template <typename ExpandFunc, typename EvalFunc>
    static std::pair<bool, std::string> try_expand_arithmetic_expression(const std::string& arg,
                                                                         size_t& i,
                                                                         ExpandFunc expand_func,
                                                                         EvalFunc eval_func);

    template <typename GetVarFunc, typename ExpandFunc>
    static std::string expand_parameter_with_default(const std::string& param_expr,
                                                     GetVarFunc get_var, ExpandFunc expand_func);

   private:
    Shell* shell;
    const std::unordered_map<std::string, std::string>& env_vars;
    bool use_exported_vars_only = false;
};

template <typename ExpandFunc, typename EvalFunc>
std::pair<bool, std::string> VariableExpander::try_expand_arithmetic_expression(
    const std::string& arg, size_t& i, ExpandFunc expand_func, EvalFunc eval_func) {
    if (!(arg[i] == '$' && i + 2 < arg.length() && arg[i + 1] == '(' && arg[i + 2] == '(')) {
        return {false, ""};
    }

    size_t start = i + 3;
    size_t paren_depth = 1;
    size_t end = start;

    while (end < arg.length() && paren_depth > 0) {
        if (arg[end] == '(' && end + 1 < arg.length() && arg[end + 1] == '(') {
            paren_depth++;
            end += 2;
        } else if (arg[end] == ')' && end + 1 < arg.length() && arg[end + 1] == ')') {
            paren_depth--;
            if (paren_depth == 0) {
                break;
            }
            end += 2;
        } else {
            end++;
        }
    }

    if (paren_depth == 0 && end + 1 < arg.length()) {
        std::string expr = arg.substr(start, end - start);
        expand_func(expr);

        try {
            long long result = eval_func(expr);
            i = end + 1;
            return {true, std::to_string(result)};
        } catch (const std::exception&) {
            i = end + 1;
            return {true, arg.substr(i - (end - start + 3), end - start + 3)};
        }
    }

    return {false, ""};
}

template <typename GetVarFunc, typename ExpandFunc>
std::string VariableExpander::expand_parameter_with_default(const std::string& param_expr,
                                                            GetVarFunc get_var,
                                                            ExpandFunc expand_func) {
    size_t colon_pos = param_expr.find(':');
    size_t dash_pos = param_expr.find('-', colon_pos != std::string::npos ? colon_pos + 1 : 0);

    if (colon_pos != std::string::npos && dash_pos != std::string::npos) {
        std::string var_name = param_expr.substr(0, colon_pos);
        std::string default_val = param_expr.substr(dash_pos + 1);
        std::string env_val = get_var(var_name);
        if (!env_val.empty()) {
            return env_val;
        }
        expand_func(default_val);
        return default_val;
    }

    if (colon_pos == std::string::npos && param_expr.find('-') != std::string::npos) {
        dash_pos = param_expr.find('-');
        std::string var_name = param_expr.substr(0, dash_pos);
        std::string default_val = param_expr.substr(dash_pos + 1);
        std::string env_val = get_var(var_name);
        if (!env_val.empty()) {
            return env_val;
        }
        expand_func(default_val);
        return default_val;
    }

    return get_var(param_expr);
}
