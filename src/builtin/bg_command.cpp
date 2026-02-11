/*
  bg_command.cpp

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

#include "bg_command.h"

#include <unistd.h>
#include <csignal>
#include <iostream>

#include "builtin_help.h"
#include "cjsh.h"
#include "error_out.h"
#include "exec.h"
#include "job_control.h"
#include "shell.h"

int bg_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args,
                            {"Usage: bg [%JOB]", "Resume a stopped job in the background."})) {
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

    const JobState current_state = job->state.load(std::memory_order_relaxed);
    if (current_state != JobState::STOPPED) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     std::to_string(job_id),
                     "not stopped",
                     {"Use 'jobs' to list job states"}});
        return 1;
    }

    if (g_shell && g_shell->shell_exec) {
        g_shell->shell_exec->set_job_output_forwarding(job->pgid, false);
    }

    if (killpg(job->pgid, SIGCONT) < 0) {
        perror("cjsh: bg: killpg");
        return 1;
    }

    job->state.store(JobState::RUNNING, std::memory_order_relaxed);
    job->stop_notified.store(false, std::memory_order_relaxed);
    std::cout << "[" << job_id << "]+ " << job->display_command() << " &" << '\n';

    return 0;
}
