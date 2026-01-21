#include "utils/pipeline_status_utils.h"

#include <cstdlib>
#include <sstream>

#include "exec.h"

namespace {

std::string build_status_string(const std::vector<int>& statuses) {
    std::stringstream builder;
    for (size_t i = 0; i < statuses.size(); ++i) {
        if (i != 0) {
            builder << ' ';
        }
        builder << statuses[i];
    }
    return builder.str();
}

}  // namespace

namespace pipeline_status_utils {

void apply_pipeline_status_env(Exec* exec_ptr,
                               const std::function<void(const std::string&)>& on_set_callback,
                               const std::function<void()>& on_unset_callback) {
    if (!exec_ptr) {
        unsetenv("PIPESTATUS");
        if (on_unset_callback) {
            on_unset_callback();
        }
        return;
    }

    const auto& pipeline_statuses = exec_ptr->get_last_pipeline_statuses();
    if (pipeline_statuses.empty()) {
        unsetenv("PIPESTATUS");
        if (on_unset_callback) {
            on_unset_callback();
        }
        return;
    }

    const std::string pipe_status_str = build_status_string(pipeline_statuses);
    setenv("PIPESTATUS", pipe_status_str.c_str(), 1);
    if (on_set_callback) {
        on_set_callback(pipe_status_str);
    }
}

}  // namespace pipeline_status_utils
