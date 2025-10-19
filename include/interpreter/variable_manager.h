#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class VariableManager {
   public:
    using VariableMap = std::unordered_map<std::string, std::string>;
    using VariableStack = std::vector<VariableMap>;
    using ExportedLocalsStack = std::vector<std::vector<std::string>>;

    VariableManager() = default;
    ~VariableManager() = default;

    VariableManager(const VariableManager&) = delete;
    VariableManager& operator=(const VariableManager&) = delete;
    VariableManager(VariableManager&&) = default;
    VariableManager& operator=(VariableManager&&) = default;

    void push_scope();
    void pop_scope();

    void set_local_variable(const std::string& name, const std::string& value);
    void set_environment_variable(const std::string& name, const std::string& value);
    bool is_local_variable(const std::string& name) const;
    bool unset_local_variable(const std::string& name);
    void mark_local_as_exported(const std::string& name);
    bool in_function_scope() const {
        return !local_variable_stack.empty();
    }

    std::string get_variable_value(const std::string& var_name) const;
    bool variable_is_set(const std::string& var_name) const;

    const VariableStack& get_local_stack() const {
        return local_variable_stack;
    }
    VariableStack& get_local_stack() {
        return local_variable_stack;
    }

   private:
    VariableStack local_variable_stack;
    ExportedLocalsStack exported_locals_stack;
    
    std::vector<std::vector<std::pair<std::string, std::string>>> saved_env_stack;

    std::string get_special_variable(const std::string& var_name) const;
    std::string get_positional_parameter(const std::string& var_name) const;
};
