#include "generate_completions_command.h"

#include "builtin_help.h"
#include "shell.h"
#include "signal_handler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "external_sub_completions.h"

// NOLINTBEGIN(performance-avoid-endl)
int generate_completions_command(const std::vector<std::string>& args, Shell* shell) {
    if (builtin_handle_help(args,
                            {"Usage: generate-completions [OPTIONS] [COMMAND ...]",
                             "Regenerate cached completion data for commands.",
                             "With no COMMAND, all executables in PATH are processed.",
                             "Options:", "  --quiet, -q       Suppress per-command output",
                             "  --no-force        Reuse existing cache entries when present",
                             "  --force, -f       Force regeneration (default)",
                             "  --jobs, -j <N>    Process up to N commands in parallel",
                             "  --                Treat remaining arguments as command names"})) {
        return 0;
    }

    if (!config::completions_enabled) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "generate-completions",
                     "completions are disabled in the current shell configuration",
                     {}});
        return 1;
    }

    bool quiet = false;
    bool force_refresh = true;
    bool after_separator = false;
    std::size_t requested_jobs = 0;
    std::vector<std::string> targets;

    auto set_job_count = [&](const std::string& value) -> bool {
        if (value.empty())
            return false;
        std::size_t parsed = 0;
        try {
            unsigned long raw = std::stoul(value);
            if (raw == 0 || raw > std::numeric_limits<std::size_t>::max())
                return false;
            parsed = static_cast<std::size_t>(raw);
        } catch (const std::exception&) {
            return false;
        }
        if (parsed == 0)
            return false;
        requested_jobs = parsed;
        return true;
    };

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (!after_separator && !arg.empty() && arg[0] == '-') {
            if (arg == "--") {
                after_separator = true;
                continue;
            }
            if (arg == "--force" || arg == "-f") {
                force_refresh = true;
                continue;
            }
            if (arg == "--no-force") {
                force_refresh = false;
                continue;
            }
            if (arg == "--quiet" || arg == "-q") {
                quiet = true;
                continue;
            }
            if (arg == "--jobs" || arg == "-j") {
                if (i + 1 >= args.size()) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "generate-completions",
                                 "missing value for " + arg,
                                 {"Pass a positive integer job count."}});
                    return 2;
                }
                ++i;
                if (!set_job_count(args[i])) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "generate-completions",
                                 "invalid job count: " + args[i],
                                 {"Use a positive integer."}});
                    return 2;
                }
                continue;
            }
            if (arg.rfind("--jobs=", 0) == 0) {
                std::string value = arg.substr(7);
                if (!set_job_count(value)) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "generate-completions",
                                 "invalid job count: " + value,
                                 {"Use a positive integer."}});
                    return 2;
                }
                continue;
            }
            if (arg.size() > 2 && arg[0] == '-' && arg[1] == 'j') {
                std::string value = arg.substr(2);
                if (value.empty()) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "generate-completions",
                                 "missing value for -j",
                                 {"Use -j <count> with a positive integer."}});
                    return 2;
                }
                if (!set_job_count(value)) {
                    print_error({ErrorType::INVALID_ARGUMENT,
                                 "generate-completions",
                                 "invalid job count: " + value,
                                 {"Use a positive integer."}});
                    return 2;
                }
                continue;
            }
            print_error({ErrorType::INVALID_ARGUMENT,
                         "generate-completions",
                         "invalid option: " + arg,
                         {"Use --help for usage."}});
            return 2;
        }
        targets.push_back(arg);
    }

    if (!cjsh_filesystem::initialize_cjsh_directories()) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "generate-completions",
                     "failed to initialize cjsh directories",
                     {}});
        return 1;
    }

    if (targets.empty()) {
        targets = cjsh_filesystem::get_executables_in_path();
    }

    if (targets.empty()) {
        if (!quiet) {
            std::cout << "generate-completions: no commands discovered" << std::endl;
        }
        return 0;
    }

    // quiet = false;

    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    const auto start_time = std::chrono::steady_clock::now();

    std::vector<std::string> failures;
    std::atomic<bool> cancel_requested{false};
    std::mutex signal_poll_mutex;
    Shell* shell_ptr = shell;

    auto check_for_interrupt = [&](bool allow_shell_processing) -> bool {
        if (cancel_requested.load()) {
            return true;
        }

#ifdef SIGINT
        if (!SignalHandler::has_pending_signals()) {
            return false;
        }

        SignalProcessingResult pending{};
        bool processed = false;

        if (allow_shell_processing && shell_ptr != nullptr) {
            pending = shell_ptr->process_pending_signals();
            processed = true;
        } else if (g_signal_handler != nullptr) {
            std::lock_guard<std::mutex> lock(signal_poll_mutex);
            pending = g_signal_handler->process_pending_signals(nullptr);
            processed = true;
        }

        if (processed && pending.sigint) {
            cancel_requested.store(true);
            return true;
        }
#endif

        return cancel_requested.load();
    };

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    std::size_t default_jobs =
        hardware_threads == 0 ? 4 : static_cast<std::size_t>(hardware_threads);
    if (default_jobs == 0)
        default_jobs = 1;

    std::size_t job_count = requested_jobs > 0 ? requested_jobs : default_jobs;
    if (job_count == 0)
        job_count = 1;
    job_count = std::min(job_count, targets.size());
    if (job_count == 0)
        job_count = 1;

    if (!quiet) {
        std::cout << "generate-completions: processing " << targets.size() << " command"
                  << (targets.size() == 1 ? "" : "s") << (force_refresh ? " (forcing refresh)" : "")
                  << " using " << job_count << " job" << (job_count == 1 ? "" : "s") << std::endl;
    }

    std::size_t success_count = 0;
    if (job_count == 1) {
        for (const auto& command : targets) {
            if (check_for_interrupt(true)) {
                break;
            }

            bool generated = regenerate_external_completion_cache(command, force_refresh);
            if (generated) {
                ++success_count;
                if (!quiet) {
                    std::cout << "  [OK] " << command << std::endl;
                }
            } else {
                failures.push_back(command);
                if (!quiet) {
                    std::cout << "  [WARN] " << command
                              << " (no manual entry or unable to generate)" << std::endl;
                }
            }

            if (check_for_interrupt(true)) {
                break;
            }
        }
    } else {
        std::atomic<std::size_t> next_index{0};
        std::atomic<std::size_t> success_counter{0};
        std::mutex output_mutex;
        std::mutex failure_mutex;

        auto worker = [&]() {
            std::vector<std::string> local_failures;
            while (true) {
                if (check_for_interrupt(false)) {
                    break;
                }

                std::size_t index = next_index.fetch_add(1, std::memory_order_relaxed);
                if (index >= targets.size())
                    break;

                if (cancel_requested.load()) {
                    break;
                }

                const std::string& command = targets[index];
                bool generated = regenerate_external_completion_cache(command, force_refresh);
                if (generated) {
                    success_counter.fetch_add(1, std::memory_order_relaxed);
                    if (!quiet) {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cout << "  [OK] " << command << std::endl;
                    }
                } else {
                    local_failures.push_back(command);
                    if (!quiet) {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        std::cout << "  [WARN] " << command
                                  << " (no manual entry or unable to generate)" << std::endl;
                    }
                }
            }

            if (!local_failures.empty()) {
                std::lock_guard<std::mutex> lock(failure_mutex);
                failures.insert(failures.end(), local_failures.begin(), local_failures.end());
            }
        };

        std::vector<std::thread> workers;
        workers.reserve(job_count);
        for (std::size_t i = 0; i < job_count; ++i) {
            workers.emplace_back(worker);
        }
        for (auto& thread : workers) {
            thread.join();
        }

        success_count = success_counter.load(std::memory_order_relaxed);
    }

    if (check_for_interrupt(true)) {
#ifdef SIGINT
        return 128 + SIGINT;
#else
        return 130;
#endif
    }

    if (!quiet) {
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        const auto elapsed_seconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(elapsed);
        std::ostringstream elapsed_stream;
        elapsed_stream << std::fixed << std::setprecision(1) << elapsed_seconds.count() << "s";

        std::cout << "generate-completions: " << success_count << "/" << targets.size()
                  << " updated";
        if (!failures.empty()) {
            std::cout << ", " << failures.size() << " missing";
        }
        std::cout << ", total time " << elapsed_stream.str() << std::endl;
        std::cout << "You may see elevated reported memory usage during this session until cjsh "
                     "is restarted because of this command."
                  << std::endl;
        std::cout << std::endl;
    }

    if (!failures.empty()) {
        if (quiet) {
            for (const auto& command : failures) {
                std::cout << command << std::endl;
            }
        }
        return 1;
    }

    return 0;
}
// NOLINTEND(performance-avoid-endl)
