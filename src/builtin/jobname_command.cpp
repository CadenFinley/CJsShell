#include "jobname_command.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#include "builtin_help.h"
#include "cjsh.h"
#include "error_out.h"
#include "job_control.h"
#include "shell.h"

namespace {

bool is_blank(const std::string& value) {
    return std::all_of(value.begin(), value.end(),
                       [](unsigned char ch) { return std::isspace(ch); });
}

std::string normalize_name(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        return {};
    }
    std::string name;
    for (size_t i = 2; i < args.size(); ++i) {
        if (i > 2) {
            name.push_back(' ');
        }
        name.append(args[i]);
    }
    return name;
}

}  // namespace

int jobname_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: jobname JOB_SPEC NEW_NAME",
                                   "Assign a temporary display name to a job."})) {
        return 0;
    }

    if (args.size() < 3) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "jobname",
                     "missing job spec or new name",
                     {"Usage: jobname JOB_SPEC NEW_NAME"}});
        return 1;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_status();

    std::vector<std::string> resolve_args = {"jobname", args[1]};
    auto resolved = job_control_helpers::resolve_control_job_target(resolve_args, job_manager);
    if (!resolved) {
        return 1;
    }

    auto job = resolved->job;
    if (!job) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     args[1],
                     "no such job",
                     {"Use 'jobs' to list available jobs"}});
        return 1;
    }

    std::string new_name = normalize_name(args);
    if (new_name.empty() || is_blank(new_name)) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "jobname",
                     "new name cannot be empty",
                     {"Provide the desired display name after the job spec"}});
        return 1;
    }

    job->set_custom_name(new_name);
    std::cout << "[" << job->job_id << "] => " << job->display_command() << '\n';
    return 0;
}
