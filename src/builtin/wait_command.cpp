#include "wait_command.h"

#include <sys/wait.h>
#include <unistd.h>
#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

int wait_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args, {"Usage: wait [ID ...]",
                   "Wait for specified jobs or processes. Without IDs, waits for all."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();

    if (args.size() == 1) {
        auto jobs = job_manager.get_all_jobs();
        int last_exit_status = 0;

        for (const auto& job : jobs) {
            if (job->state != JobState::RUNNING) {
                continue;
            }

            auto job_exit = job_control_helpers::wait_for_job_and_remove(job, job_manager);
            if (job_exit.has_value()) {
                last_exit_status = job_exit.value();
            }
        }

        return last_exit_status;
    }

    int last_exit_status = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& target = args[i];

        if (!target.empty() && target[0] == '%') {
            auto parsed_job_id = job_control_helpers::parse_job_specifier(target);
            if (!parsed_job_id.has_value()) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "Arguments must be process or job IDs",
                             {"Use 'jobs' to list available jobs"}});
                return 1;
            }

            int job_id = parsed_job_id.value();
            auto job = job_manager.get_job(job_id);
            if (!job) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "no such job",
                             {"Use 'jobs' to list available jobs"}});
                return 1;
            }

            auto job_exit = job_control_helpers::wait_for_job_and_remove(job, job_manager);
            if (job_exit.has_value()) {
                last_exit_status = job_exit.value();
            }
        } else {
            try {
                pid_t pid = std::stoi(target);
                int status = 0;
                if (waitpid(pid, &status, 0) < 0) {
                    perror("wait");
                    return 1;
                }

                auto interpreted = job_control_helpers::interpret_wait_status(status);
                if (interpreted.has_value()) {
                    last_exit_status = interpreted.value();
                }

                job_manager.mark_pid_completed(pid, status);
            } catch (...) {
                print_error({ErrorType::INVALID_ARGUMENT,
                             target,
                             "Arguments must be process or job IDs",
                             {"Use 'jobs' to list available jobs"}});
                return 1;
            }
        }
    }

    return last_exit_status;
}
