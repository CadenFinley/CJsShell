#pragma once

#include <functional>
#include <string>

class Exec;

namespace pipeline_status_utils {

void apply_pipeline_status_env(Exec* exec_ptr,
                               const std::function<void(const std::string&)>& on_set_callback = {},
                               const std::function<void()>& on_unset_callback = {});

}  // namespace pipeline_status_utils
