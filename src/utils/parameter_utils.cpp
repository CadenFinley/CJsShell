#include "utils/parameter_utils.h"

#include <unistd.h>
#include <cstdlib>
#include <string>

#include "job_control.h"
#include "shell.h"

namespace parameter_utils {

std::string join_positional_parameters(const Shell* shell) {
    if (shell == nullptr) {
        return "";
    }

    const auto params = shell->get_positional_parameters();
    if (params.empty()) {
        return "";
    }

    size_t total_length = 0;
    for (const auto& param : params) {
        total_length += param.size();
    }
    if (params.size() > 1) {
        total_length += params.size() - 1;
    }

    std::string joined;
    joined.reserve(total_length);

    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            joined.push_back(' ');
        }
        joined += params[i];
    }
    return joined;
}

std::string get_last_background_pid_string() {
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

}  // namespace parameter_utils
