/*
  shell_hooks.cpp

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

#include "shell.h"

#include <algorithm>

namespace {

constexpr size_t to_index(HookType type) {
    return static_cast<size_t>(type);
}

constexpr std::array<HookTypeDescriptor, static_cast<size_t>(HookType::Count)>
    kHookTypeDescriptors = {
        {{HookType::Precmd, "precmd"}, {HookType::Preexec, "preexec"}, {HookType::Chpwd, "chpwd"}}};

}  // namespace

const std::array<HookTypeDescriptor, static_cast<size_t>(HookType::Count)>&
get_hook_type_descriptors() {
    return kHookTypeDescriptors;
}

std::optional<HookType> parse_hook_type(const std::string& name) {
    for (const auto& descriptor : kHookTypeDescriptors) {
        if (name == descriptor.name) {
            return descriptor.type;
        }
    }
    return std::nullopt;
}

void Shell::register_hook(HookType hook_type, const std::string& function_name) {
    if (function_name.empty()) {
        return;
    }

    auto& hook_list = hooks[to_index(hook_type)];

    if (std::find(hook_list.begin(), hook_list.end(), function_name) == hook_list.end()) {
        hook_list.push_back(function_name);
    }
}

void Shell::unregister_hook(HookType hook_type, const std::string& function_name) {
    auto& hook_list = hooks[to_index(hook_type)];
    hook_list.erase(std::remove(hook_list.begin(), hook_list.end(), function_name),
                    hook_list.end());
}

std::vector<std::string> Shell::get_hooks(HookType hook_type) const {
    return hooks[to_index(hook_type)];
}

void Shell::clear_hooks(HookType hook_type) {
    hooks[to_index(hook_type)].clear();
}

void Shell::execute_hooks(HookType hook_type) {
    const auto& hook_list = hooks[to_index(hook_type)];
    if (hook_list.empty()) {
        return;
    }

    for (const auto& function_name : hook_list) {
        execute(function_name);
    }
}
