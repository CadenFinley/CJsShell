/*
  disown_command.cpp

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

#include "disown_command.h"

#include "builtin_help.h"
#include "cjsh.h"
#include "error_out.h"
#include "exec.h"
#include "job_control.h"
#include "shell.h"

int disown_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: disown [jobspec ...]",
                                   "Remove jobs from the shell's job table so they are not"
                                   " sent hangup signals."})) {
        return 0;
    }

    auto& job_manager = JobManager::instance();
    job_manager.update_job_statuses();

    bool disown_all = false;
    std::vector<int> targets;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-a" || args[i] == "--all") {
            disown_all = true;
            continue;
        }

        std::string job_spec = args[i];
        if (!job_spec.empty() && job_spec[0] == '%') {
            job_spec = job_spec.substr(1);
        }

        try {
            targets.push_back(std::stoi(job_spec));
        } catch (...) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         args[i],
                         "no such job",
                         {"Use 'jobs' to list available jobs"}});
            return 1;
        }
    }

    if (disown_all) {
        auto jobs = job_manager.get_all_jobs();
        targets.clear();
        targets.reserve(jobs.size());
        for (const auto& job : jobs) {
            targets.push_back(job->job_id);
        }
    }

    if (targets.empty()) {
        int current = job_manager.get_current_job();
        if (current == -1) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         "",
                         "no current job",
                         {"Use 'jobs' to identify targets"}});
            return 1;
        }
        targets.push_back(current);
    }

    bool had_error = false;
    for (int job_id : targets) {
        auto job = job_manager.get_job(job_id);
        if (!job) {
            print_error({ErrorType::INVALID_ARGUMENT,
                         std::to_string(job_id),
                         "no such job",
                         {"Use 'jobs' to list available jobs"}});
            had_error = true;
            continue;
        }

        job_manager.remove_job(job_id);
        if (g_shell && g_shell->shell_exec) {
            g_shell->shell_exec->remove_job(job_id);
        }
    }

    return had_error ? 1 : 0;
}
