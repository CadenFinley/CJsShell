#include "exit_command.h"

#include "builtin_help.h"
#include "error_out.h"
#include "job_control.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "cjsh.h"

int exit_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, {"Usage: exit [-f|--force] [N]",
                                   "Exit the shell with status N (default last command).",
                                   "Use --force to skip running exit traps."})) {
        return 0;
    }
    int exit_code = 0;
    bool force_exit = false;

    int non_flag_args = 0;

    force_exit = std::find(args.begin(), args.end(), "-f") != args.end() ||
                 std::find(args.begin(), args.end(), "--force") != args.end();

    for (size_t i = 1; i < args.size(); i++) {
        const std::string& val = args[i];
        if (val != "-f" && val != "--force") {
            non_flag_args++;
            char* endptr = nullptr;
            long code = std::strtol(val.c_str(), &endptr, 10);
            if (endptr && *endptr == '\0') {
                exit_code = static_cast<int>(code) & 0xFF;

                break;
            } else {
                g_exit_flag = true;
                setenv("EXIT_CODE", "128", 1);
                return 0;
            }
        }
    }

    if (non_flag_args > 1) {
        g_exit_flag = true;
        setenv("EXIT_CODE", "128", 1);
        return 0;
    }

    if (!force_exit) {
        auto& job_manager = JobManager::instance();
        job_manager.update_job_status();

        const auto jobs = job_manager.get_all_jobs();
        bool has_stopped_jobs = false;
        bool has_running_jobs = false;

        for (const auto& job : jobs) {
            if (!job) {
                continue;
            }
            if (job->state == JobState::STOPPED) {
                has_stopped_jobs = true;
            } else if (job->state == JobState::RUNNING) {
                has_running_jobs = true;
            }
        }

        if (has_stopped_jobs || has_running_jobs) {
            std::string warning;
            if (has_stopped_jobs && has_running_jobs) {
                warning = "There are stopped and running jobs.";
            } else if (has_stopped_jobs) {
                warning = "There are stopped jobs.";
            } else {
                warning = "There are running jobs.";
            }

            print_error({ErrorType::RUNTIME_ERROR,
                         ErrorSeverity::WARNING,
                         "exit",
                         warning,
                         {"Use `jobs` to inspect them.",
                          "Resume, disown, or run `exit --force` to exit."}});
            return 1;
        }
    }

    if (force_exit) {
        cleanup_resources();
        std::exit(exit_code);
    }

    g_exit_flag = true;
    setenv("EXIT_CODE", std::to_string(exit_code).c_str(), 1);
    return 0;
}
