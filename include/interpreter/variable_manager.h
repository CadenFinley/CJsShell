#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class VariableManager {
   public:
    using VariableMap = std::unordered_map<std::string, std::string>;
    using VariableStack = std::vector<VariableMap>;

    VariableManager() = default;
    ~VariableManager() = default;

    VariableManager(const VariableManager&) = delete;
    VariableManager& operator=(const VariableManager&) = delete;
    VariableManager(VariableManager&&) = default;
    VariableManager& operator=(VariableManager&&) = default;

    void push_scope();
    void pop_scope();

    void set_local_variable(const std::string& name, const std::string& value);
    bool is_local_variable(const std::string& name) const;

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

    std::string get_special_variable(const std::string& var_name) const;
    std::string get_positional_parameter(const std::string& var_name) const;
};
