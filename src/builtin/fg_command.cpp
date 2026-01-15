#include "fg_command.h"

#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

int fg_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: fg [%JOB]", "Bring a job to the foreground."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_status();

    auto resolved_job = job_control_helpers::resolve_control_job_target(args, job_manager);
    if (!resolved_job) {
        return 1;
    }

    auto job = resolved_job->job;
    int job_id = resolved_job->job_id;

    if (isatty(STDIN_FILENO) != 0) {
        if (tcsetpgrp(STDIN_FILENO, job->pgid) < 0) {
            perror("fg: tcsetpgrp");
            return 1;
        }
    }

    if (job->state == JobState::DONE || job->state == JobState::TERMINATED) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::to_string(job_id),
                     "job has already completed",
                     {"Use 'jobs' to list available jobs"}});
        return 1;
    }

    if (killpg(job->pgid, SIGCONT) < 0) {
        perror("fg: killpg");
        return 1;
    }

    job->state = JobState::RUNNING;
    job->stop_notified = false;
    job_manager.set_current_job(job_id);

    std::cout << job->command << '\n';

    int status = 0;
    for (pid_t pid : job->pids) {
        waitpid(pid, &status, WUNTRACED);
    }

    if (isatty(STDIN_FILENO) != 0) {
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }

    if (WIFEXITED(status)) {
        job_manager.remove_job(job_id);
        return WEXITSTATUS(status);
    }
    if (WIFSTOPPED(status)) {
        job->state = JobState::STOPPED;
        job_manager.notify_job_stopped(job);
        return 128 + WSTOPSIG(status);
    }
    if (WIFSIGNALED(status)) {
        job_manager.remove_job(job_id);
        return 128 + WTERMSIG(status);
    }

    return 0;
}
