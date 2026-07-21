/*
  generate_completions_command.cpp

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

#include "generate_completions_command.h"

#include "builtin_help.h"
#include "shell.h"
#include "signal_handler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "cjsh_filesystem.h"
#include "error_out.h"
#include "external_sub_completions.h"
#include "shell_env.h"

namespace {

constexpr const char kCommandName[] = "generate-completions";
constexpr const char kAnsiClearLine[] = "\x1b[2K";
constexpr std::size_t kFallbackDefaultJobs = 4;
constexpr int kMinimumProgressColumns = 20;

struct GenerateCompletionsOptions {
    bool quiet{false};
    bool force_refresh{true};
    bool include_subcommands{false};
    std::size_t requested_jobs{0};
    std::vector<std::string> targets;
};

struct TerminalDimensions {
    int columns{0};
};

bool stdout_supports_ansi_progress() {
#ifndef _WIN32
    const char* term = std::getenv("TERM");
    if (term != nullptr && std::string(term) == "dumb")
        return false;

    return isatty(STDOUT_FILENO) != 0;
#else
    return false;
#endif
}

bool query_stdout_terminal_dimensions(TerminalDimensions& dimensions) {
#ifndef _WIN32
    struct winsize size {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0)
        return false;

    dimensions.columns = static_cast<int>(size.ws_col);
    return true;
#else
    (void)dimensions;
    return false;
#endif
}

std::string truncate_progress_text(const std::string& text, std::size_t width) {
    if (text.size() <= width)
        return text;
    if (width == 0)
        return {};
    if (width <= 3)
        return text.substr(0, width);

    return text.substr(0, width - 3) + "...";
}

std::string format_target_result_line(const std::string& target_name, bool generated,
                                      bool is_root_target) {
    std::ostringstream line;
    if (generated) {
        line << "  [OK] " << target_name;
    } else {
        line << "  [WARN] " << target_name << " (no manual entry or unable to generate)";
    }

    if (!is_root_target)
        line << " (subcommand cache)";

    return line.str();
}

class GenerateCompletionsProgressDisplay {
   public:
    GenerateCompletionsProgressDisplay(bool requested, std::size_t total)
        : total_(total), enabled_(requested && total > 0 && stdout_supports_ansi_progress()) {
        TerminalDimensions dimensions;
        if (!enabled_ || !query_stdout_terminal_dimensions(dimensions) ||
            dimensions.columns < kMinimumProgressColumns) {
            enabled_ = false;
            return;
        }

        columns_ = dimensions.columns;
        render_locked();
    }

    ~GenerateCompletionsProgressDisplay() { finish(); }

    GenerateCompletionsProgressDisplay(const GenerateCompletionsProgressDisplay&) = delete;
    GenerateCompletionsProgressDisplay& operator=(const GenerateCompletionsProgressDisplay&) =
        delete;

    bool enabled() const { return enabled_; }

    void report_result(const std::string& target_name, bool generated, bool is_root_target) {
        if (!enabled_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (finished_)
            return;

        std::cout << '\r' << kAnsiClearLine
                  << format_target_result_line(target_name, generated, is_root_target) << '\n';

        last_target_ = target_name;
        last_target_is_root_ = is_root_target;

        if (is_root_target) {
            if (completed_ < total_)
                ++completed_;
            if (!generated)
                ++missing_;
        } else {
            ++subcommand_count_;
        }

        render_locked();
    }

    void finish() {
        if (!enabled_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (finished_)
            return;

        refresh_dimensions_locked();
        std::cout << '\r' << kAnsiClearLine << std::flush;
        finished_ = true;
    }

   private:
    void refresh_dimensions_locked() {
        TerminalDimensions dimensions;
        if (query_stdout_terminal_dimensions(dimensions) &&
            dimensions.columns >= kMinimumProgressColumns) {
            columns_ = dimensions.columns;
        }
    }

    std::string build_bar_locked(std::size_t width) const {
        if (width == 0)
            return {};

        const std::size_t completed = std::min(completed_, total_);
        std::size_t filled = total_ == 0 ? width : (completed * width) / total_;
        if (completed > 0 && filled == 0)
            filled = 1;
        if (completed >= total_)
            filled = width;

        std::string bar(filled, '#');
        bar.append(width - filled, '-');
        return bar;
    }

    std::string build_progress_line_locked() const {
        const std::size_t terminal_width = static_cast<std::size_t>(columns_ > 0 ? columns_ : 80);
        const std::size_t width = terminal_width > 1 ? terminal_width - 1 : terminal_width;
        const std::size_t completed = std::min(completed_, total_);
        const std::size_t percent = total_ == 0 ? 100 : (completed * 100) / total_;

        std::ostringstream status;
        status << completed << "/" << total_ << " " << percent << "%";
        if (missing_ > 0)
            status << " " << missing_ << " missing";
        if (subcommand_count_ > 0)
            status << " " << subcommand_count_ << " sub";

        if (width < 50)
            return truncate_progress_text(std::string(kCommandName) + " " + status.str(), width);

        std::size_t bar_width = 12;
        if (width >= 100) {
            bar_width = 32;
        } else if (width >= 80) {
            bar_width = 28;
        } else if (width >= 64) {
            bar_width = 18;
        }

        std::string line = std::string(kCommandName) + " [" + build_bar_locked(bar_width) +
                           "] " + status.str();

        std::string label;
        if (!last_target_.empty()) {
            label = last_target_is_root_ ? last_target_ : "sub " + last_target_;
        } else {
            label = "starting";
        }

        if (!label.empty() && line.size() + 1 < width) {
            line.push_back(' ');
            line += truncate_progress_text(label, width - line.size());
        }

        return truncate_progress_text(line, width);
    }

    void render_locked() {
        refresh_dimensions_locked();
        if (columns_ < kMinimumProgressColumns)
            return;

        const std::string line = build_progress_line_locked();
        std::cout << '\r' << kAnsiClearLine << line << std::flush;
    }

    std::mutex mutex_;
    std::size_t total_{0};
    std::size_t completed_{0};
    std::size_t missing_{0};
    std::size_t subcommand_count_{0};
    std::string last_target_;
    bool last_target_is_root_{true};
    bool enabled_{false};
    bool finished_{false};
    int columns_{0};
};

void print_invalid_option_error(const std::string& option) {
    print_error({ErrorType::INVALID_ARGUMENT,
                 kCommandName,
                 "invalid option: " + option,
                 {"Use --help for usage."}});
}

void print_invalid_jobs_argument(const std::string& option_name, const std::string& value) {
    print_error({ErrorType::INVALID_ARGUMENT,
                 kCommandName,
                 "invalid argument for " + option_name + ": " + value,
                 {"Use a positive integer."}});
}

void print_missing_jobs_argument(const std::string& option_name) {
    print_error({ErrorType::INVALID_ARGUMENT,
                 kCommandName,
                 "missing value for " + option_name,
                 {"Use " + option_name + " <count> with a positive integer."}});
}

bool parse_job_count(const std::string& value, std::size_t& parsed_jobs) {
    if (value.empty())
        return false;

    std::size_t consumed = 0;
    try {
        const unsigned long long raw = std::stoull(value, &consumed);
        if (consumed != value.size() || raw == 0 ||
            raw > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
            return false;
        }
        parsed_jobs = static_cast<std::size_t>(raw);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int parse_generate_completions_options(const std::vector<std::string>& args,
                                       GenerateCompletionsOptions& options) {
    bool after_separator = false;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (!after_separator && !arg.empty() && arg[0] == '-') {
            if (arg == "--") {
                after_separator = true;
                continue;
            }
            if (arg == "--force" || arg == "-f") {
                options.force_refresh = true;
                continue;
            }
            if (arg == "--no-force") {
                options.force_refresh = false;
                continue;
            }
            if (arg == "--quiet" || arg == "-q") {
                options.quiet = true;
                continue;
            }
            if (arg == "--subcommands" || arg == "-s") {
                options.include_subcommands = true;
                continue;
            }
            if (arg == "--jobs" || arg == "-j") {
                if (i + 1 >= args.size()) {
                    print_missing_jobs_argument(arg);
                    return 2;
                }

                ++i;
                if (!parse_job_count(args[i], options.requested_jobs)) {
                    print_invalid_jobs_argument(arg, args[i]);
                    return 2;
                }
                continue;
            }
            if (arg.rfind("--jobs=", 0) == 0) {
                const std::string value = arg.substr(7);
                if (value.empty()) {
                    print_missing_jobs_argument("--jobs");
                    return 2;
                }
                if (!parse_job_count(value, options.requested_jobs)) {
                    print_invalid_jobs_argument("--jobs", value);
                    return 2;
                }
                continue;
            }
            if (arg.size() > 2 && arg[0] == '-' && arg[1] == 'j') {
                const std::string value = arg.substr(2);
                if (value.empty()) {
                    print_missing_jobs_argument("-j");
                    return 2;
                }
                if (!parse_job_count(value, options.requested_jobs)) {
                    print_invalid_jobs_argument("-j", value);
                    return 2;
                }
                continue;
            }

            print_invalid_option_error(arg);
            return 2;
        }

        options.targets.push_back(arg);
    }

    return 0;
}

std::size_t resolve_job_count(std::size_t requested_jobs, std::size_t target_count) {
    std::size_t job_count = requested_jobs;
    if (job_count == 0) {
        const unsigned int hardware_threads = std::thread::hardware_concurrency();
        job_count = hardware_threads == 0 ? kFallbackDefaultJobs
                                          : static_cast<std::size_t>(hardware_threads);
    }

    if (job_count == 0)
        job_count = 1;

    if (target_count > 0)
        job_count = std::min(job_count, target_count);

    if (job_count == 0)
        job_count = 1;

    return job_count;
}

void print_target_result_line(const std::string& target_name, bool generated, bool is_root_target) {
    std::cout << format_target_result_line(target_name, generated, is_root_target) << std::endl;
}

void report_target_result(bool quiet, std::mutex* output_mutex, const std::string& target_name,
                          bool generated, bool is_root_target,
                          GenerateCompletionsProgressDisplay* progress_display) {
    if (quiet)
        return;

    if (progress_display != nullptr && progress_display->enabled()) {
        progress_display->report_result(target_name, generated, is_root_target);
        return;
    }

    if (output_mutex != nullptr) {
        std::lock_guard<std::mutex> lock(*output_mutex);
        print_target_result_line(target_name, generated, is_root_target);
        return;
    }

    print_target_result_line(target_name, generated, is_root_target);
}

int interrupt_exit_code() {
#ifdef SIGINT
    return 128 + SIGINT;
#else
    return 130;
#endif
}

}  // namespace

// NOLINTBEGIN(performance-avoid-endl)
int generate_completions_command(const std::vector<std::string>& args, Shell* shell) {
    auto run = [&]() -> int {
        if (builtin_handle_help(
                args, {"Usage: generate-completions [OPTIONS] [COMMAND ...]",
                       "Regenerate cached completion data for commands.",
                       "With no COMMAND, all executables in PATH are processed.",
                       "Options:", "  --quiet, -q       Suppress per-command output",
                       "  --no-force        Reuse existing cache entries when present",
                       "  --force, -f       Force regeneration (default)",
                       "  --subcommands, -s Also generate caches for discovered subcommands",
                       "  --jobs, -j <N>    Process up to N commands in parallel",
                       "  --                Treat remaining arguments as command names",
                       "IMPORTANT: This can take significant time and system resources.",
                       "Tip: Use -j/--jobs to tune parallelism (defaults to CPU count).",
                       "Progress is redrawn beneath live status lines when stdout is a terminal."})) {
            return 0;
        }

        if (!config::completions_enabled) {
            print_error({ErrorType::RUNTIME_ERROR,
                         kCommandName,
                         "completions are disabled in the current shell configuration",
                         {}});
            return 1;
        }

        GenerateCompletionsOptions options;
        const int parse_status = parse_generate_completions_options(args, options);
        if (parse_status != 0)
            return parse_status;

        if (!cjsh_filesystem::initialize_cjsh_directories()) {
            print_error({ErrorType::RUNTIME_ERROR,
                         kCommandName,
                         "failed to initialize cjsh directories",
                         {}});
            return 1;
        }

        if (options.targets.empty()) {
            options.targets = cjsh_filesystem::get_executables_in_path();
        }

        if (options.targets.empty()) {
            if (!options.quiet) {
                std::cout << kCommandName << ": no commands discovered" << std::endl;
            }
            return 0;
        }

        std::sort(options.targets.begin(), options.targets.end());
        options.targets.erase(std::unique(options.targets.begin(), options.targets.end()),
                              options.targets.end());

        const auto start_time = std::chrono::steady_clock::now();

        std::vector<std::string> failures;
        std::atomic<bool> cancel_requested{false};
        std::mutex signal_poll_mutex;

        auto check_for_interrupt = [&](bool allow_shell_processing) -> bool {
            if (cancel_requested.load()) {
                return true;
            }

            if (cjsh_env::exit_requested()) {
                cancel_requested.store(true);
                return true;
            }

#ifdef SIGINT
            if (!SignalHandler::has_pending_signals()) {
                return false;
            }

            SignalProcessingResult pending{};
            bool processed = false;

            if (allow_shell_processing && shell != nullptr) {
                pending = shell->process_pending_signals();
                processed = true;
            } else if (auto* signal_handler = SignalHandler::instance()) {
                std::lock_guard<std::mutex> lock(signal_poll_mutex);
                pending = signal_handler->process_pending_signals(nullptr);
                processed = true;
            }

            if (processed) {
                if (pending.sigint || pending.sigterm || pending.sighup) {
                    cancel_requested.store(true);
                    return true;
                }
#ifdef SIGTSTP
                if (std::find(pending.trapped_signals.begin(), pending.trapped_signals.end(),
                              SIGTSTP) != pending.trapped_signals.end()) {
                    cancel_requested.store(true);
                    return true;
                }
#endif
            }
#endif

            return cancel_requested.load();
        };

        const std::size_t job_count =
            resolve_job_count(options.requested_jobs, options.targets.size());

        if (!options.quiet) {
            std::cout << kCommandName << ": processing " << options.targets.size() << " command"
                      << (options.targets.size() == 1 ? "" : "s")
                      << (options.force_refresh ? " (forcing refresh)" : "")
                      << (options.include_subcommands ? " with subcommands" : "") << " using "
                      << job_count << " job" << (job_count == 1 ? "" : "s") << std::endl;
        }

        GenerateCompletionsProgressDisplay progress_display(!options.quiet,
                                                            options.targets.size());

        std::size_t success_count = 0;
        if (job_count == 1) {
            for (const auto& command : options.targets) {
                if (check_for_interrupt(true)) {
                    break;
                }

                auto progress_report = [&](const std::string& target_name, bool generated,
                                           bool is_root_target) {
                    report_target_result(options.quiet, nullptr, target_name, generated,
                                         is_root_target, &progress_display);
                };

                auto should_cancel = [&]() { return check_for_interrupt(true); };

                bool generated = regenerate_external_completion_cache(
                    command, options.force_refresh, options.include_subcommands, progress_report,
                    should_cancel);
                if (generated) {
                    ++success_count;
                } else {
                    failures.push_back(command);
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
                    if (index >= options.targets.size())
                        break;

                    if (cancel_requested.load()) {
                        break;
                    }

                    const std::string& command = options.targets[index];
                    auto progress_report = [&](const std::string& target_name, bool generated,
                                               bool is_root_target) {
                        report_target_result(options.quiet, &output_mutex, target_name, generated,
                                             is_root_target, &progress_display);
                    };

                    auto should_cancel = [&]() { return check_for_interrupt(false); };

                    bool generated = regenerate_external_completion_cache(
                        command, options.force_refresh, options.include_subcommands,
                        progress_report, should_cancel);
                    if (generated) {
                        success_counter.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        local_failures.push_back(command);
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

        progress_display.finish();

        if (check_for_interrupt(true)) {
            return interrupt_exit_code();
        }

        if (!options.quiet) {
            const auto elapsed = std::chrono::steady_clock::now() - start_time;
            const auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(elapsed);
            std::ostringstream elapsed_stream;
            elapsed_stream << std::fixed << std::setprecision(1) << elapsed_seconds.count() << "s";

            std::cout << kCommandName << ": " << success_count << "/" << options.targets.size()
                      << " updated";
            if (!failures.empty()) {
                std::cout << ", " << failures.size() << " missing";
            }
            std::cout << ", total time " << elapsed_stream.str() << std::endl;
            std::cout
                << "You may see elevated reported memory usage during this session until cjsh "
                   "is restarted because of this command."
                << std::endl;
            std::cout << std::endl;
        }

        if (!failures.empty()) {
            if (options.quiet) {
                for (const auto& command : failures) {
                    std::cout << command << std::endl;
                }
            }
            return 1;
        }

        return 0;
    };

    try {
        return run();
    } catch (...) {
        print_error({ErrorType::INVALID_ARGUMENT, kCommandName, "invalid argument", {}});
        return 1;
    }
}
// NOLINTEND(performance-avoid-endl)
