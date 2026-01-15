#include "shell.h"

#include <algorithm>

void Shell::register_hook(const std::string& hook_type, const std::string& function_name) {
    if (function_name.empty()) {
        return;
    }

    auto& hook_list = hooks[hook_type];

    if (std::find(hook_list.begin(), hook_list.end(), function_name) == hook_list.end()) {
        hook_list.push_back(function_name);
    }
}

void Shell::unregister_hook(const std::string& hook_type, const std::string& function_name) {
    auto it = hooks.find(hook_type);
    if (it == hooks.end()) {
        return;
    }

    auto& hook_list = it->second;
    hook_list.erase(std::remove(hook_list.begin(), hook_list.end(), function_name),
                    hook_list.end());

    if (hook_list.empty()) {
        hooks.erase(it);
    }
}

std::vector<std::string> Shell::get_hooks(const std::string& hook_type) const {
    auto it = hooks.find(hook_type);
    if (it != hooks.end()) {
        return it->second;
    }
    return {};
}

void Shell::clear_hooks(const std::string& hook_type) {
    hooks.erase(hook_type);
}

void Shell::execute_hooks(const std::string& hook_type) {
    auto it = hooks.find(hook_type);
    if (it == hooks.end()) {
        return;
    }

    const auto& hook_list = it->second;
    if (hook_list.empty()) {
        return;
    }

    for (const auto& function_name : hook_list) {
        execute(function_name);
    }
}
