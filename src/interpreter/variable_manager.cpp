#include "variable_manager.h"

#include <unistd.h>

#include <cctype>
#include <cstdlib>

#include "cjsh.h"
#include "readonly_command.h"
#include "shell.h"
#include "utils/parameter_utils.h"

void VariableManager::push_scope() {
    local_variable_stack.emplace_back();
    exported_locals_stack.emplace_back();
    saved_env_stack.emplace_back();
}

void VariableManager::pop_scope() {
    if (!saved_env_stack.empty()) {
        for (const auto& [var_name, old_value] : saved_env_stack.back()) {
            if (old_value.empty()) {
                unsetenv(var_name.c_str());
            } else {
                setenv(var_name.c_str(), old_value.c_str(), 1);
            }
        }
        saved_env_stack.pop_back();
    }

    if (!exported_locals_stack.empty()) {
        exported_locals_stack.pop_back();
    }

    if (!local_variable_stack.empty()) {
        local_variable_stack.pop_back();
    }
}

void VariableManager::set_local_variable(const std::string& name, const std::string& value) {
    if (local_variable_stack.empty()) {
        set_environment_variable(name, value);
    } else {
        local_variable_stack.back()[name] = value;
    }
}

void VariableManager::set_environment_variable(const std::string& name, const std::string& value) {
    if (g_shell) {
        auto& env_vars = g_shell->get_env_vars();
        env_vars[name] = value;

        if (name == "PATH" || name == "PWD" || name == "HOME" || name == "USER" ||
            name == "SHELL") {
            setenv(name.c_str(), value.c_str(), 1);
        }

        auto* shell_parser = g_shell->get_parser();
        if (shell_parser) {
            shell_parser->set_env_vars(env_vars);
        }
    }
}

bool VariableManager::is_local_variable(const std::string& name) const {
    if (!local_variable_stack.empty()) {
        const auto& current_scope = local_variable_stack.back();
        return current_scope.find(name) != current_scope.end();
    }
    return false;
}

bool VariableManager::unset_local_variable(const std::string& name) {
    if (!local_variable_stack.empty()) {
        auto& current_scope = local_variable_stack.back();
        auto it = current_scope.find(name);
        if (it != current_scope.end()) {
            current_scope.erase(it);
            return true;
        }
    }
    return false;
}

void VariableManager::mark_local_as_exported(const std::string& name) {
    if (!exported_locals_stack.empty() && !saved_env_stack.empty()) {
        const char* old_val = getenv(name.c_str());
        std::string old_value = (old_val != nullptr) ? old_val : "";

        saved_env_stack.back().emplace_back(name, old_value);
        exported_locals_stack.back().push_back(name);
    }
}

bool VariableManager::in_function_scope() const {
    return !local_variable_stack.empty();
}

std::string VariableManager::get_variable_value(const std::string& var_name) const {
    if (!local_variable_stack.empty()) {
        const auto& current_scope = local_variable_stack.back();
        auto it = current_scope.find(var_name);
        if (it != current_scope.end()) {
            return it->second;
        }
    }

    std::string special_var = get_special_variable(var_name);
    if (!special_var.empty() || var_name == "?" || var_name == "$" || var_name == "#" ||
        var_name == "*" || var_name == "@" || var_name == "!") {
        return special_var;
    }

    std::string positional = get_positional_parameter(var_name);
    if (!positional.empty() || (var_name.length() == 1 && isdigit(var_name[0]) != 0)) {
        return positional;
    }

    if (g_shell) {
        const auto& env_vars = g_shell->get_env_vars();
        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
            return it->second;
        }
    }

    const char* env_val = getenv(var_name.c_str());
    return (env_val != nullptr) ? env_val : "";
}

bool VariableManager::variable_is_set(const std::string& var_name) const {
    if (!local_variable_stack.empty()) {
        const auto& current_scope = local_variable_stack.back();
        if (current_scope.find(var_name) != current_scope.end()) {
            return true;
        }
    }

    if (var_name == "?" || var_name == "$" || var_name == "#" || var_name == "*" ||
        var_name == "@" || var_name == "!") {
        return true;
    }

    if (var_name.length() == 1 && isdigit(var_name[0]) != 0) {
        if (getenv(var_name.c_str()) != nullptr) {
            return true;
        }

        int param_num = var_name[0] - '0';
        if (g_shell && param_num > 0) {
            auto params = g_shell->get_positional_parameters();
            return static_cast<size_t>(param_num - 1) < params.size();
        }
        return false;
    }

    if (g_shell) {
        const auto& env_vars = g_shell->get_env_vars();
        if (env_vars.find(var_name) != env_vars.end()) {
            return true;
        }
    }

    return getenv(var_name.c_str()) != nullptr;
}

std::string VariableManager::get_special_variable(const std::string& var_name) const {
    if (var_name == "?") {
        const char* status_env = getenv("?");
        return (status_env != nullptr) ? status_env : "0";
    }
    if (var_name == "$") {
        return std::to_string(getpid());
    }
    if (var_name == "#") {
        if (g_shell) {
            return std::to_string(g_shell->get_positional_parameter_count());
        }
        return "0";
    }
    if (var_name == "*" || var_name == "@") {
        return parameter_utils::join_positional_parameters(g_shell ? g_shell.get() : nullptr);
    }
    if (var_name == "!") {
        return parameter_utils::get_last_background_pid_string();
    }
    return "";
}

std::string VariableManager::get_positional_parameter(const std::string& var_name) const {
    if (var_name.length() == 1 && isdigit(var_name[0]) != 0) {
        const char* env_val = getenv(var_name.c_str());
        if (env_val != nullptr) {
            return env_val;
        }

        int param_num = var_name[0] - '0';
        if (g_shell && param_num > 0) {
            auto params = g_shell->get_positional_parameters();
            if (static_cast<size_t>(param_num - 1) < params.size()) {
                return params[param_num - 1];
            }
        }
    }
    return "";
}
