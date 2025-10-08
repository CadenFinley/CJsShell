#include "variable_manager.h"

#include <unistd.h>

#include <cctype>
#include <cstdlib>

#include "cjsh.h"
#include "job_control.h"
#include "readonly_command.h"
#include "shell.h"

void VariableManager::push_scope() {
    local_variable_stack.emplace_back();
}

void VariableManager::pop_scope() {
    if (!local_variable_stack.empty()) {
        local_variable_stack.pop_back();
    }
}

void VariableManager::set_local_variable(const std::string& name, const std::string& value) {
    if (local_variable_stack.empty()) {
        if (g_shell) {
            auto& env_vars = g_shell->get_env_vars();
            env_vars[name] = value;

            if (name == "PATH" || name == "PWD" || name == "HOME" || name == "USER" ||
                name == "SHELL") {
                setenv(name.c_str(), value.c_str(), 1);
            }
        }
    } else {
        local_variable_stack.back()[name] = value;
    }
}

bool VariableManager::is_local_variable(const std::string& name) const {
    if (!local_variable_stack.empty()) {
        const auto& current_scope = local_variable_stack.back();
        return current_scope.find(name) != current_scope.end();
    }
    return false;
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
        if (g_shell) {
            auto params = g_shell->get_positional_parameters();
            std::string result;
            for (size_t i = 0; i < params.size(); ++i) {
                if (i > 0)
                    result += " ";
                result += params[i];
            }
            return result;
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
