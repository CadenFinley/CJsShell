/*
  fg_command.cpp

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

#include "fg_command.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

int fg_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: fg [%JOB]", "Bring a job to the foreground."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();

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

    std::cout << job->display_command() << '\n';

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
