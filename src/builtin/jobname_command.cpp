/*
  jobname_command.cpp

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

#include "jobname_command.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

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
    job_manager.update_job_statuses();

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
