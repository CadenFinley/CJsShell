/*
  variable_manager.cpp

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "variable_manager.h"

#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <unordered_set>

#include "arithmetic_evaluator.h"
#include "flags.h"
#include "parameter_utils.h"
#include "parser_utils.h"
#include "readonly_command.h"
#include "shell.h"
#include "shell_env.h"

namespace {

bool is_array_join_index(const std::string& index) {
    return index == "@" || index == "*";
}

}  // namespace

void VariableManager::push_scope() {
    local_variable_stack.emplace_back();
    local_array_stack.emplace_back();
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
    if (!local_array_stack.empty()) {
        local_array_stack.pop_back();
    }
}

void VariableManager::set_local_variable(const std::string& name, const std::string& value) {
    if (local_variable_stack.empty()) {
        assign_variable(name, value, false);
        return;
    }

    ParsedArrayReference parsed;
    if (parse_array_reference(name, parsed) && parsed.has_index) {
        (void)assign_array_element_value(parsed.name, parsed.index, value, false, true);
        return;
    }

    if (has_local_array_binding(name)) {
        (void)assign_scalar_value(name, value, false, true);
        return;
    }

    local_variable_stack.back()[name] = value;
}

void VariableManager::set_environment_variable(const std::string& name, const std::string& value) {
    if (g_shell) {
        auto& env_vars = cjsh_env::env_vars();
        env_vars[name] = value;
        cjsh_env::mirror_set_to_process_env(name, value);

        global_array_variables.erase(name);

        auto* shell_parser = g_shell->get_parser();
        if (shell_parser) {
            shell_parser->set_env_vars(env_vars);
        }
    }
}

bool VariableManager::assign_variable(const std::string& target, const std::string& value,
                                      bool append) {
    ParsedArrayReference parsed;
    if (!parse_array_reference(target, parsed)) {
        return false;
    }

    bool local_scope = should_assign_to_local_scope(parsed.name);
    if (parsed.has_index) {
        return assign_array_element_value(parsed.name, parsed.index, value, append, local_scope);
    }

    return assign_scalar_value(parsed.name, value, append, local_scope);
}

bool VariableManager::assign_global_variable(const std::string& target, const std::string& value,
                                             bool append) {
    ParsedArrayReference parsed;
    if (!parse_array_reference(target, parsed)) {
        return false;
    }

    if (parsed.has_index) {
        return assign_array_element_value(parsed.name, parsed.index, value, append, false);
    }

    return assign_scalar_value(parsed.name, value, append, false);
}

bool VariableManager::assign_array_literal(const std::string& name,
                                           const std::vector<std::string>& words, bool append) {
    if (!is_valid_identifier(name)) {
        return false;
    }

    bool local_scope = should_assign_to_local_scope(name);
    IndexedArray* target_array = nullptr;

    if (local_scope) {
        if (local_array_stack.empty()) {
            return false;
        }

        auto& arrays = local_array_stack.back();
        auto& scalars = local_variable_stack.back();
        auto scalar_it = scalars.find(name);

        auto [array_it, inserted] = arrays.emplace(name, IndexedArray{});
        target_array = &array_it->second;

        if (append) {
            if (inserted && scalar_it != scalars.end()) {
                target_array->emplace(0, scalar_it->second);
                scalars.erase(scalar_it);
            }
        } else {
            target_array->clear();
            if (scalar_it != scalars.end()) {
                scalars.erase(scalar_it);
            }
        }
    } else {
        auto [array_it, inserted] = global_array_variables.emplace(name, IndexedArray{});
        target_array = &array_it->second;

        if (append) {
            if (inserted && has_global_scalar_binding(name)) {
                target_array->emplace(0, get_global_scalar_value(name));
                remove_global_scalar_binding(name);
            }
        } else {
            target_array->clear();
            remove_global_scalar_binding(name);
        }
    }

    if (target_array == nullptr) {
        return false;
    }

    long long cursor = 0;
    if (append && !target_array->empty()) {
        cursor = target_array->rbegin()->first + 1;
    }

    for (const std::string& word : words) {
        bool explicit_element = false;
        bool element_append = false;
        std::string index_expr;
        std::string element_value;

        if (word.size() >= 4 && word.front() == '[') {
            size_t close_bracket = word.find(']');
            if (close_bracket != std::string::npos && close_bracket > 1) {
                if (close_bracket + 1 < word.size() && word[close_bracket + 1] == '=') {
                    explicit_element = true;
                    index_expr = word.substr(1, close_bracket - 1);
                    element_value = word.substr(close_bracket + 2);
                } else if (close_bracket + 2 < word.size() && word[close_bracket + 1] == '+' &&
                           word[close_bracket + 2] == '=') {
                    explicit_element = true;
                    element_append = true;
                    index_expr = word.substr(1, close_bracket - 1);
                    element_value = word.substr(close_bracket + 3);
                }
            }
        }

        if (explicit_element) {
            if (is_array_join_index(index_expr)) {
                return false;
            }

            auto index = evaluate_array_index_expression(index_expr);
            if (!index.has_value()) {
                return false;
            }

            if (element_append) {
                (*target_array)[*index] += element_value;
            } else {
                (*target_array)[*index] = element_value;
            }
            cursor = *index + 1;
            continue;
        }

        (*target_array)[cursor] = word;
        cursor++;
    }

    return true;
}

bool VariableManager::assign_global_array_literal(const std::string& name,
                                                  const std::vector<std::string>& words,
                                                  bool append) {
    if (!is_valid_identifier(name)) {
        return false;
    }

    auto [array_it, inserted] = global_array_variables.emplace(name, IndexedArray{});
    IndexedArray* target_array = &array_it->second;

    if (append) {
        if (inserted && has_global_scalar_binding(name)) {
            target_array->emplace(0, get_global_scalar_value(name));
            remove_global_scalar_binding(name);
        }
    } else {
        target_array->clear();
        remove_global_scalar_binding(name);
    }

    long long cursor = 0;
    if (append && !target_array->empty()) {
        cursor = target_array->rbegin()->first + 1;
    }

    for (const std::string& word : words) {
        bool explicit_element = false;
        bool element_append = false;
        std::string index_expr;
        std::string element_value;

        if (word.size() >= 4 && word.front() == '[') {
            size_t close_bracket = word.find(']');
            if (close_bracket != std::string::npos && close_bracket > 1) {
                if (close_bracket + 1 < word.size() && word[close_bracket + 1] == '=') {
                    explicit_element = true;
                    index_expr = word.substr(1, close_bracket - 1);
                    element_value = word.substr(close_bracket + 2);
                } else if (close_bracket + 2 < word.size() && word[close_bracket + 1] == '+' &&
                           word[close_bracket + 2] == '=') {
                    explicit_element = true;
                    element_append = true;
                    index_expr = word.substr(1, close_bracket - 1);
                    element_value = word.substr(close_bracket + 3);
                }
            }
        }

        if (explicit_element) {
            if (is_array_join_index(index_expr)) {
                return false;
            }

            auto index = evaluate_array_index_expression(index_expr);
            if (!index.has_value()) {
                return false;
            }

            if (element_append) {
                (*target_array)[*index] += element_value;
            } else {
                (*target_array)[*index] = element_value;
            }
            cursor = *index + 1;
            continue;
        }

        (*target_array)[cursor] = word;
        cursor++;
    }

    return true;
}

bool VariableManager::is_local_variable(const std::string& name) const {
    if (local_variable_stack.empty()) {
        return false;
    }

    const auto& current_scope = local_variable_stack.back();
    if (current_scope.find(name) != current_scope.end()) {
        return true;
    }

    return has_local_array_binding(name);
}

bool VariableManager::unset_local_variable(const std::string& name) {
    if (local_variable_stack.empty()) {
        return false;
    }

    bool removed = false;
    auto& current_scope = local_variable_stack.back();
    auto scalar_it = current_scope.find(name);
    if (scalar_it != current_scope.end()) {
        current_scope.erase(scalar_it);
        removed = true;
    }

    if (!local_array_stack.empty()) {
        auto& current_arrays = local_array_stack.back();
        auto array_it = current_arrays.find(name);
        if (array_it != current_arrays.end()) {
            current_arrays.erase(array_it);
            removed = true;
        }
    }

    return removed;
}

bool VariableManager::unset_variable(const std::string& target) {
    ParsedArrayReference parsed;
    if (!parse_array_reference(target, parsed)) {
        return false;
    }

    bool local_scope = should_assign_to_local_scope(parsed.name);

    if (parsed.has_index) {
        if (is_array_join_index(parsed.index)) {
            if (local_scope) {
                bool removed = unset_local_variable(parsed.name);
                return removed;
            }

            bool removed = false;
            if (global_array_variables.erase(parsed.name) > 0) {
                removed = true;
            }
            if (has_global_scalar_binding(parsed.name)) {
                remove_global_scalar_binding(parsed.name);
                removed = true;
            }
            return removed;
        }

        auto index = evaluate_array_index_expression(parsed.index);
        if (!index.has_value()) {
            return false;
        }

        if (local_scope) {
            IndexedArray* array = get_local_array(parsed.name);
            if (array == nullptr) {
                return false;
            }
            return array->erase(*index) > 0;
        }

        IndexedArray* array = get_global_array(parsed.name);
        if (array == nullptr) {
            return false;
        }
        return array->erase(*index) > 0;
    }

    if (local_scope) {
        return unset_local_variable(parsed.name);
    }

    bool removed = false;
    if (global_array_variables.erase(parsed.name) > 0) {
        removed = true;
    }
    if (has_global_scalar_binding(parsed.name)) {
        remove_global_scalar_binding(parsed.name);
        removed = true;
    }
    return removed;
}

void VariableManager::mark_local_as_exported(const std::string& name) {
    if (!exported_locals_stack.empty() && !saved_env_stack.empty()) {
        // Raw getenv here: variable manager mirrors process env.
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
    ParsedArrayReference parsed;
    if (parse_array_reference(var_name, parsed) && parsed.has_index) {
        if (!local_array_stack.empty()) {
            const auto& local_scalars = local_variable_stack.back();
            auto local_scalar_it = local_scalars.find(parsed.name);
            const IndexedArray* local_array = get_local_array(parsed.name);

            if (local_array != nullptr) {
                if (is_array_join_index(parsed.index)) {
                    return join_array_values(*local_array);
                }

                auto index = evaluate_array_index_expression(parsed.index);
                if (!index.has_value()) {
                    return "";
                }

                auto value_it = local_array->find(*index);
                if (value_it != local_array->end()) {
                    return value_it->second;
                }
                return "";
            }

            if (local_scalar_it != local_scalars.end()) {
                if (is_array_join_index(parsed.index)) {
                    return local_scalar_it->second;
                }

                auto index = evaluate_array_index_expression(parsed.index);
                if (index.has_value() && *index == 0) {
                    return local_scalar_it->second;
                }
                return "";
            }
        }

        const IndexedArray* global_array = get_global_array(parsed.name);
        if (global_array != nullptr) {
            if (is_array_join_index(parsed.index)) {
                return join_array_values(*global_array);
            }

            auto index = evaluate_array_index_expression(parsed.index);
            if (!index.has_value()) {
                return "";
            }

            auto value_it = global_array->find(*index);
            if (value_it != global_array->end()) {
                return value_it->second;
            }
            return "";
        }

        if (has_global_scalar_binding(parsed.name)) {
            std::string scalar_value = get_global_scalar_value(parsed.name);
            if (is_array_join_index(parsed.index)) {
                return scalar_value;
            }

            auto index = evaluate_array_index_expression(parsed.index);
            if (index.has_value() && *index == 0) {
                return scalar_value;
            }
            return "";
        }

        return "";
    }

    if (!local_variable_stack.empty()) {
        const auto& current_scope = local_variable_stack.back();
        auto scalar_it = current_scope.find(var_name);
        if (scalar_it != current_scope.end()) {
            return scalar_it->second;
        }

        const IndexedArray* local_array = get_local_array(var_name);
        if (local_array != nullptr) {
            auto zero_it = local_array->find(0);
            if (zero_it != local_array->end()) {
                return zero_it->second;
            }
        }
    }

    std::string special_var = get_special_variable(var_name);
    if (!special_var.empty() || parameter_utils::is_named_special_parameter_name(var_name)) {
        return special_var;
    }

    std::string positional = get_positional_parameter(var_name);
    if (!positional.empty() || (var_name.length() == 1 && isdigit(var_name[0]) != 0)) {
        return positional;
    }

    const IndexedArray* global_array = get_global_array(var_name);
    if (global_array != nullptr) {
        auto zero_it = global_array->find(0);
        if (zero_it != global_array->end()) {
            return zero_it->second;
        }
        return "";
    }

    if (g_shell) {
        const auto& env_vars = cjsh_env::env_vars();
        auto it = env_vars.find(var_name);
        if (it != env_vars.end()) {
            return it->second;
        }
    }

    // Raw getenv here: variable manager mirrors process env.
    const char* env_val = getenv(var_name.c_str());
    return (env_val != nullptr) ? env_val : "";
}

bool VariableManager::variable_is_set(const std::string& var_name) const {
    ParsedArrayReference parsed;
    if (parse_array_reference(var_name, parsed) && parsed.has_index) {
        const bool is_join = is_array_join_index(parsed.index);

        if (!local_variable_stack.empty()) {
            const auto& local_scalars = local_variable_stack.back();
            auto scalar_it = local_scalars.find(parsed.name);
            const IndexedArray* local_array = get_local_array(parsed.name);

            if (local_array != nullptr) {
                if (is_join) {
                    return !local_array->empty();
                }

                auto index = evaluate_array_index_expression(parsed.index);
                return index.has_value() && (local_array->find(*index) != local_array->end());
            }

            if (scalar_it != local_scalars.end()) {
                if (is_join) {
                    return true;
                }

                auto index = evaluate_array_index_expression(parsed.index);
                return index.has_value() && *index == 0;
            }
        }

        const IndexedArray* global_array = get_global_array(parsed.name);
        if (global_array != nullptr) {
            if (is_join) {
                return !global_array->empty();
            }

            auto index = evaluate_array_index_expression(parsed.index);
            return index.has_value() && (global_array->find(*index) != global_array->end());
        }

        if (has_global_scalar_binding(parsed.name)) {
            if (is_join) {
                return true;
            }

            auto index = evaluate_array_index_expression(parsed.index);
            return index.has_value() && *index == 0;
        }

        return false;
    }

    if (!local_variable_stack.empty()) {
        const auto& current_scope = local_variable_stack.back();
        if (current_scope.find(var_name) != current_scope.end()) {
            return true;
        }

        const IndexedArray* local_array = get_local_array(var_name);
        if (local_array != nullptr && local_array->find(0) != local_array->end()) {
            return true;
        }
    }

    if (parameter_utils::is_named_special_parameter_name(var_name)) {
        return true;
    }

    if (var_name.length() == 1 && isdigit(var_name[0]) != 0) {
        // Raw getenv here: positional vars stored in process env.
        if (getenv(var_name.c_str()) != nullptr) {
            return true;
        }

        int param_num = var_name[0] - '0';
        if (param_num > 0) {
            auto params = flags::get_positional_parameters();
            return static_cast<size_t>(param_num - 1) < params.size();
        }
        return false;
    }

    const IndexedArray* global_array = get_global_array(var_name);
    if (global_array != nullptr && global_array->find(0) != global_array->end()) {
        return true;
    }

    if (g_shell) {
        const auto& env_vars = cjsh_env::env_vars();
        if (env_vars.find(var_name) != env_vars.end()) {
            return true;
        }
    }

    // Raw getenv here: variable manager mirrors process env.
    return getenv(var_name.c_str()) != nullptr;
}

std::optional<size_t> VariableManager::get_array_length(const std::string& var_name) const {
    ParsedArrayReference parsed;
    if (!parse_array_reference(var_name, parsed) || !parsed.has_index ||
        !is_array_join_index(parsed.index)) {
        return std::nullopt;
    }

    const IndexedArray* local_array = get_local_array(parsed.name);
    if (local_array != nullptr) {
        return local_array->size();
    }
    if (!local_variable_stack.empty() &&
        local_variable_stack.back().find(parsed.name) != local_variable_stack.back().end()) {
        return 1;
    }

    const IndexedArray* global_array = get_global_array(parsed.name);
    if (global_array != nullptr) {
        return global_array->size();
    }
    if (has_global_scalar_binding(parsed.name)) {
        return 1;
    }

    return std::nullopt;
}

std::string VariableManager::get_array_keys(const std::string& var_name) const {
    ParsedArrayReference parsed;
    if (!parse_array_reference(var_name, parsed) || !parsed.has_index ||
        !is_array_join_index(parsed.index)) {
        return "";
    }

    const IndexedArray* local_array = get_local_array(parsed.name);
    if (local_array != nullptr) {
        return join_array_keys(*local_array);
    }
    if (!local_variable_stack.empty() &&
        local_variable_stack.back().find(parsed.name) != local_variable_stack.back().end()) {
        return "0";
    }

    const IndexedArray* global_array = get_global_array(parsed.name);
    if (global_array != nullptr) {
        return join_array_keys(*global_array);
    }
    if (has_global_scalar_binding(parsed.name)) {
        return "0";
    }

    return "";
}

std::vector<std::string> VariableManager::get_variable_names() const {
    std::unordered_set<std::string> name_set;

    for (const auto& scope : local_variable_stack) {
        for (const auto& entry : scope) {
            name_set.insert(entry.first);
        }
    }
    for (const auto& scope : local_array_stack) {
        for (const auto& entry : scope) {
            name_set.insert(entry.first);
        }
    }

    for (const auto& entry : global_array_variables) {
        name_set.insert(entry.first);
    }

    if (g_shell) {
        const auto& env_vars = cjsh_env::env_vars();
        for (const auto& entry : env_vars) {
            name_set.insert(entry.first);
        }
    }

    std::vector<std::string> names;
    names.reserve(name_set.size());
    for (const auto& name : name_set) {
        names.push_back(name);
    }
    return names;
}

bool VariableManager::parse_array_reference(const std::string& target,
                                            ParsedArrayReference& parsed) const {
    std::string trimmed = trim_whitespace(target);
    if (trimmed.empty()) {
        return false;
    }

    size_t left_bracket = trimmed.find('[');
    if (left_bracket == std::string::npos) {
        if (!is_valid_identifier(trimmed)) {
            return false;
        }
        parsed.name = trimmed;
        parsed.index.clear();
        parsed.has_index = false;
        return true;
    }

    if (trimmed.back() != ']') {
        return false;
    }

    std::string name = trimmed.substr(0, left_bracket);
    if (!is_valid_identifier(name)) {
        return false;
    }

    std::string index = trimmed.substr(left_bracket + 1, trimmed.length() - left_bracket - 2);
    if (index.empty()) {
        return false;
    }

    parsed.name = std::move(name);
    parsed.index = std::move(index);
    parsed.has_index = true;
    return true;
}

std::optional<long long> VariableManager::evaluate_array_index_expression(
    const std::string& expr) const {
    std::string trimmed = trim_whitespace(expr);
    if (trimmed.empty() || is_array_join_index(trimmed)) {
        return std::nullopt;
    }

    auto var_reader = [this](const std::string& name) -> long long {
        std::string value = get_variable_value(name);
        if (value.empty()) {
            return 0;
        }
        try {
            return std::stoll(value);
        } catch (const std::exception&) {
            return 0;
        }
    };
    auto var_writer = [](const std::string&, long long) {};

    ArithmeticEvaluator evaluator(var_reader, var_writer);
    try {
        long long result = evaluator.evaluate(trimmed);
        if (result < 0) {
            return std::nullopt;
        }
        return result;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string VariableManager::join_array_values(const IndexedArray& array) const {
    if (array.empty()) {
        return "";
    }

    std::string joined;
    bool first = true;
    for (const auto& [_, value] : array) {
        if (!first) {
            joined.push_back(' ');
        }
        joined += value;
        first = false;
    }
    return joined;
}

std::string VariableManager::join_array_keys(const IndexedArray& array) const {
    if (array.empty()) {
        return "";
    }

    std::string joined;
    bool first = true;
    for (const auto& [index, _] : array) {
        if (!first) {
            joined.push_back(' ');
        }
        joined += std::to_string(index);
        first = false;
    }
    return joined;
}

bool VariableManager::has_local_array_binding(const std::string& name) const {
    if (local_array_stack.empty()) {
        return false;
    }
    return local_array_stack.back().find(name) != local_array_stack.back().end();
}

bool VariableManager::has_local_binding(const std::string& name) const {
    if (local_variable_stack.empty()) {
        return false;
    }
    if (local_variable_stack.back().find(name) != local_variable_stack.back().end()) {
        return true;
    }
    return has_local_array_binding(name);
}

bool VariableManager::should_assign_to_local_scope(const std::string& name) const {
    if (local_variable_stack.empty()) {
        return false;
    }
    return has_local_binding(name);
}

VariableManager::IndexedArray* VariableManager::get_local_array(const std::string& name) {
    if (local_array_stack.empty()) {
        return nullptr;
    }
    auto it = local_array_stack.back().find(name);
    if (it == local_array_stack.back().end()) {
        return nullptr;
    }
    return &it->second;
}

const VariableManager::IndexedArray* VariableManager::get_local_array(
    const std::string& name) const {
    if (local_array_stack.empty()) {
        return nullptr;
    }
    auto it = local_array_stack.back().find(name);
    if (it == local_array_stack.back().end()) {
        return nullptr;
    }
    return &it->second;
}

VariableManager::IndexedArray* VariableManager::get_global_array(const std::string& name) {
    auto it = global_array_variables.find(name);
    if (it == global_array_variables.end()) {
        return nullptr;
    }
    return &it->second;
}

const VariableManager::IndexedArray* VariableManager::get_global_array(
    const std::string& name) const {
    auto it = global_array_variables.find(name);
    if (it == global_array_variables.end()) {
        return nullptr;
    }
    return &it->second;
}

bool VariableManager::assign_scalar_value(const std::string& name, const std::string& value,
                                          bool append, bool local_scope) {
    if (local_scope) {
        if (local_variable_stack.empty()) {
            return false;
        }

        IndexedArray* local_array = get_local_array(name);
        if (local_array != nullptr) {
            if (append) {
                (*local_array)[0] += value;
            } else {
                (*local_array)[0] = value;
            }
            return true;
        }

        auto& scalars = local_variable_stack.back();
        if (append) {
            scalars[name] += value;
        } else {
            scalars[name] = value;
        }
        return true;
    }

    IndexedArray* global_array = get_global_array(name);
    if (global_array != nullptr) {
        if (append) {
            (*global_array)[0] += value;
        } else {
            (*global_array)[0] = value;
        }
        return true;
    }

    if (append && has_global_scalar_binding(name)) {
        std::string current_value = get_global_scalar_value(name);
        set_environment_variable(name, current_value + value);
        return true;
    }

    set_environment_variable(name, value);
    return true;
}

bool VariableManager::assign_array_element_value(const std::string& name,
                                                 const std::string& index_expr,
                                                 const std::string& value, bool append,
                                                 bool local_scope) {
    if (is_array_join_index(index_expr)) {
        return false;
    }

    auto index = evaluate_array_index_expression(index_expr);
    if (!index.has_value()) {
        return false;
    }

    if (local_scope) {
        if (local_array_stack.empty()) {
            return false;
        }

        auto& arrays = local_array_stack.back();
        auto& scalars = local_variable_stack.back();
        auto scalar_it = scalars.find(name);

        auto [array_it, inserted] = arrays.emplace(name, IndexedArray{});
        IndexedArray& target = array_it->second;
        if (inserted && scalar_it != scalars.end()) {
            target.emplace(0, scalar_it->second);
            scalars.erase(scalar_it);
        }

        if (append) {
            target[*index] += value;
        } else {
            target[*index] = value;
        }
        return true;
    }

    auto [array_it, inserted] = global_array_variables.emplace(name, IndexedArray{});
    IndexedArray& target = array_it->second;

    if (inserted && has_global_scalar_binding(name)) {
        target.emplace(0, get_global_scalar_value(name));
        remove_global_scalar_binding(name);
    }

    if (append) {
        target[*index] += value;
    } else {
        target[*index] = value;
    }

    return true;
}

bool VariableManager::has_global_scalar_binding(const std::string& name) const {
    if (g_shell) {
        const auto& env_vars = cjsh_env::env_vars();
        if (env_vars.find(name) != env_vars.end()) {
            return true;
        }
    }

    // Raw getenv here: variable manager mirrors process env.
    return getenv(name.c_str()) != nullptr;
}

std::string VariableManager::get_global_scalar_value(const std::string& name) const {
    if (g_shell) {
        const auto& env_vars = cjsh_env::env_vars();
        auto it = env_vars.find(name);
        if (it != env_vars.end()) {
            return it->second;
        }
    }

    // Raw getenv here: variable manager mirrors process env.
    const char* value = getenv(name.c_str());
    return (value != nullptr) ? value : "";
}

void VariableManager::remove_global_scalar_binding(const std::string& name) {
    if (g_shell) {
        auto& env_vars = cjsh_env::env_vars();
        env_vars.erase(name);
        if (auto* parser = g_shell->get_parser()) {
            parser->set_env_vars(env_vars);
        }
    }

    unsetenv(name.c_str());
    cjsh_env::mirror_unset_from_process_env(name);
}

std::string VariableManager::get_special_variable(const std::string& var_name) const {
    return parameter_utils::get_special_parameter_value(var_name);
}

std::string VariableManager::get_positional_parameter(const std::string& var_name) const {
    if (var_name.length() == 1 && isdigit(var_name[0]) != 0) {
        // Raw getenv here: positional vars stored in process env.
        const char* env_val = getenv(var_name.c_str());
        if (env_val != nullptr) {
            return env_val;
        }

        int param_num = var_name[0] - '0';
        if (param_num > 0) {
            auto params = flags::get_positional_parameters();
            if (static_cast<size_t>(param_num - 1) < params.size()) {
                return params[static_cast<size_t>(param_num - 1)];
            }
        }
    }
    return "";
}
