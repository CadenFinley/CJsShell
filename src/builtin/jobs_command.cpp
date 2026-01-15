#include "jobs_command.h"

#include <iomanip>
#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

int jobs_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(
            args, {"Usage: jobs [-lp]", "List active jobs. -l shows PIDs, -p prints PIDs only."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_status();

    bool long_format = false;
    bool pid_only = false;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-l") {
            long_format = true;
        } else if (args[i] == "-p") {
            pid_only = true;
        } else if (args[i].substr(0, 1) == "-") {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[i],
                         "Invalid option",
                         {"Use -l for long format, -p for PIDs only"}});
            return 1;
        }
    }

    auto jobs = job_manager.get_all_jobs();
    if (jobs.empty()) {
        if (!pid_only) {
            std::cout << "No jobs" << '\n';
        }
        return 0;
    }

    int current = job_manager.get_current_job();
    int previous = job_manager.get_previous_job();

    for (const auto& job : jobs) {
        if (pid_only) {
            for (pid_t pid : job->pids) {
                std::cout << pid << '\n';
            }
            continue;
        }

        std::string status_char = " ";
        if (job->job_id == current) {
            status_char = "+";
        } else if (job->job_id == previous) {
            status_char = "-";
        }

        std::string state_str;
        switch (job->state) {
            case JobState::RUNNING:
                state_str = "Running";
                break;
            case JobState::STOPPED:
                state_str = "Stopped";
                break;
            case JobState::DONE:
                state_str = "Done";
                break;
            case JobState::TERMINATED:
                state_str = "Terminated";
                break;
        }

        std::cout << "[" << job->job_id << "]" << status_char << " ";

        if (long_format) {
            std::cout << std::setw(8) << job->pids[0] << " ";
        }

        std::cout << std::setw(12) << std::left << state_str << " " << job->command << '\n';

        job->notified = true;
    }

    return 0;
}
