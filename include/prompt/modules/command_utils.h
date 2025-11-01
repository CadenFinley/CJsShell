#pragma once

#include <exception>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "exec.h"

namespace prompt_modules {
namespace detail {

inline std::string trim_newlines(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

inline std::string command_output_trimmed(const std::string& command) {
    return exec_utils::execute_command_for_output_trimmed(command);
}

inline std::string command_output_trimmed(const std::vector<std::string>& args) {
    auto result = exec_utils::execute_command_vector_for_output(args);
    if (!result.success) {
        return "";
    }
    return trim_newlines(std::move(result.output));
}

inline std::string command_output_or(const std::string& command, std::string_view fallback) {
    std::string output = command_output_trimmed(command);
    return output.empty() ? std::string(fallback) : output;
}

inline std::string command_output_or(const std::vector<std::string>& args,
                                     std::string_view fallback) {
    std::string output = command_output_trimmed(args);
    return output.empty() ? std::string(fallback) : output;
}

inline float command_output_float_or(const std::string& command, float fallback) {
    const std::string output = command_output_trimmed(command);
    if (output.empty()) {
        return fallback;
    }

    try {
        return std::stof(output);
    } catch (const std::exception&) {
        return fallback;
    }
}

inline float command_output_float_or(const std::vector<std::string>& args, float fallback) {
    const std::string output = command_output_trimmed(args);
    if (output.empty()) {
        return fallback;
    }

    try {
        return std::stof(output);
    } catch (const std::exception&) {
        return fallback;
    }
}

inline bool command_output_matches_char(const std::string& command, char expected) {
    const std::string output = command_output_trimmed(command);
    return !output.empty() && output.front() == expected;
}

inline bool command_output_matches_char(const std::vector<std::string>& args, char expected) {
    const std::string output = command_output_trimmed(args);
    return !output.empty() && output.front() == expected;
}

inline std::string first_command_output(std::initializer_list<std::string> commands) {
    for (const auto& command : commands) {
        std::string output = command_output_trimmed(command);
        if (!output.empty()) {
            return output;
        }
    }
    return "";
}

inline exec_utils::CommandOutput command_execute(const std::string& command) {
    return exec_utils::execute_command_for_output(command);
}

inline exec_utils::CommandOutput command_execute(const std::vector<std::string>& args) {
    return exec_utils::execute_command_vector_for_output(args);
}
}  // namespace detail
}  // namespace prompt_modules
