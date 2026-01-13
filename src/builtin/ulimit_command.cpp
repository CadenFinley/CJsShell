#include "ulimit_command.h"

#include "builtin_help.h"
#include "error_out.h"

#include <sys/resource.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct OptionDescriptor {
    const char* description;
    const char* long_opt;
    rlim_t multiplier;
    int resource;
    char short_opt;
    bool available;
};

#define CJSH_ULIMIT_SUPPORTED(res, opt, long_opt, desc, mult) {desc, long_opt, mult, res, opt, true}
#define CJSH_ULIMIT_UNSUPPORTED(opt, long_opt, desc, mult) {desc, long_opt, mult, -1, opt, false}

static const OptionDescriptor kOptionTable[] = {
#ifdef RLIMIT_SBSIZE
    CJSH_ULIMIT_SUPPORTED(RLIMIT_SBSIZE, 'b', "socket-buffers", "Maximum size of socket buffers",
                          1024),
#else
    CJSH_ULIMIT_UNSUPPORTED('b', "socket-buffers", "Maximum size of socket buffers", 1024),
#endif
    CJSH_ULIMIT_SUPPORTED(RLIMIT_CORE, 'c', "core-size", "Maximum size of core files created",
                          1024),
    CJSH_ULIMIT_SUPPORTED(RLIMIT_DATA, 'd', "data-size", "Maximum size of a process's data segment",
                          1024),
#ifdef RLIMIT_NICE
    CJSH_ULIMIT_SUPPORTED(RLIMIT_NICE, 'e', "nice", "Control of maximum nice priority", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('e', "nice", "Control of maximum nice priority", 1),
#endif
    CJSH_ULIMIT_SUPPORTED(RLIMIT_FSIZE, 'f', "file-size",
                          "Maximum size of files created by the shell", 1024),
#ifdef RLIMIT_SIGPENDING
    CJSH_ULIMIT_SUPPORTED(RLIMIT_SIGPENDING, 'i', "pending-signals",
                          "Maximum number of pending signals", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('i', "pending-signals", "Maximum number of pending signals", 1),
#endif
#ifdef RLIMIT_MEMLOCK
    CJSH_ULIMIT_SUPPORTED(RLIMIT_MEMLOCK, 'l', "lock-size",
                          "Maximum size that may be locked into memory", 1024),
#else
    CJSH_ULIMIT_UNSUPPORTED('l', "lock-size", "Maximum size that may be locked into memory", 1024),
#endif
#ifdef RLIMIT_RSS
    CJSH_ULIMIT_SUPPORTED(RLIMIT_RSS, 'm', "resident-set-size", "Maximum resident set size", 1024),
#else
    CJSH_ULIMIT_UNSUPPORTED('m', "resident-set-size", "Maximum resident set size", 1024),
#endif
    CJSH_ULIMIT_SUPPORTED(RLIMIT_NOFILE, 'n', "file-descriptor-count",
                          "Maximum number of open file descriptors", 1),
#ifdef RLIMIT_MSGQUEUE
    CJSH_ULIMIT_SUPPORTED(RLIMIT_MSGQUEUE, 'q', "queue-size",
                          "Maximum bytes in POSIX message queues", 1024),
#else
    CJSH_ULIMIT_UNSUPPORTED('q', "queue-size", "Maximum bytes in POSIX message queues", 1024),
#endif
#ifdef RLIMIT_RTPRIO
    CJSH_ULIMIT_SUPPORTED(RLIMIT_RTPRIO, 'r', "realtime-priority",
                          "Maximum realtime scheduling priority", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('r', "realtime-priority", "Maximum realtime scheduling priority", 1),
#endif
    CJSH_ULIMIT_SUPPORTED(RLIMIT_STACK, 's', "stack-size", "Maximum stack size", 1024),
    CJSH_ULIMIT_SUPPORTED(RLIMIT_CPU, 't', "cpu-time", "Maximum amount of CPU time in seconds", 1),
#ifdef RLIMIT_NPROC
    CJSH_ULIMIT_SUPPORTED(RLIMIT_NPROC, 'u', "process-count",
                          "Maximum number of processes available to the current user", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('u', "process-count",
                            "Maximum number of processes available to the current user", 1),
#endif
#ifdef RLIMIT_AS
    CJSH_ULIMIT_SUPPORTED(RLIMIT_AS, 'v', "virtual-memory-size",
                          "Maximum amount of virtual memory available to each process", 1024),
#else
    CJSH_ULIMIT_UNSUPPORTED('v', "virtual-memory-size",
                            "Maximum amount of virtual memory available to each process", 1024),
#endif
#ifdef RLIMIT_SWAP
    CJSH_ULIMIT_SUPPORTED(RLIMIT_SWAP, 'w', "swap-size", "Maximum swap space", 1024),
#else
    CJSH_ULIMIT_UNSUPPORTED('w', "swap-size", "Maximum swap space", 1024),
#endif
#ifdef RLIMIT_RTTIME
    CJSH_ULIMIT_SUPPORTED(RLIMIT_RTTIME, 'y', "realtime-maxtime",
                          "Maximum contiguous realtime CPU time", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('y', "realtime-maxtime", "Maximum contiguous realtime CPU time", 1),
#endif
#ifdef RLIMIT_KQUEUES
    CJSH_ULIMIT_SUPPORTED(RLIMIT_KQUEUES, 'K', "kernel-queues", "Maximum number of kqueues", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('K', "kernel-queues", "Maximum number of kqueues", 1),
#endif
#ifdef RLIMIT_NPTS
    CJSH_ULIMIT_SUPPORTED(RLIMIT_NPTS, 'P', "ptys", "Maximum number of pseudo-terminals", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('P', "ptys", "Maximum number of pseudo-terminals", 1),
#endif
#ifdef RLIMIT_NTHR
    CJSH_ULIMIT_SUPPORTED(RLIMIT_NTHR, 'T', "threads", "Maximum number of simultaneous threads", 1),
#else
    CJSH_ULIMIT_UNSUPPORTED('T', "threads", "Maximum number of simultaneous threads", 1),
#endif
};

#undef CJSH_ULIMIT_SUPPORTED
#undef CJSH_ULIMIT_UNSUPPORTED

const std::vector<std::string>& ulimit_help_text() {
    static const std::vector<std::string> kHelpText = {
        "Usage: ulimit [options] [limit]",
        "Display or change resource limits for the current shell.",
        "",
        "Options:",
        "  -a, --all          list all current limits",
        "  -H, --hard         operate on hard limits",
        "  -S, --soft         operate on soft limits",
        "  -f, --file-size    select limit for files created by the shell (default)",
        "  -n, --file-descriptor-count  select the open file descriptor limit",
        "  --help             display this help and exit",
        "",
        "Limits can be numeric values, or the keywords 'unlimited', 'hard', or 'soft'."};
    return kHelpText;
}

const OptionDescriptor* find_by_short_option(char opt) {
    for (const auto& entry : kOptionTable) {
        if (entry.short_opt == opt) {
            return &entry;
        }
    }
    return nullptr;
}

const OptionDescriptor* find_by_long_option(const std::string& name) {
    for (const auto& entry : kOptionTable) {
        if (entry.long_opt != nullptr && name == entry.long_opt) {
            return &entry;
        }
    }
    return nullptr;
}

const OptionDescriptor* find_default_descriptor() {
    const OptionDescriptor* entry = find_by_short_option('f');
    if (entry != nullptr && entry->available) {
        return entry;
    }
    for (const auto& candidate : kOptionTable) {
        if (candidate.available) {
            return &candidate;
        }
    }
    return nullptr;
}

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool fetch_limits(const OptionDescriptor& entry, struct rlimit& limits) {
    if (!entry.available) {
        return false;
    }
    if (getrlimit(entry.resource, &limits) != 0) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "ulimit",
                     std::string("getrlimit failed: ") + std::strerror(errno),
                     {}});
        return false;
    }
    return true;
}

bool print_limit(const OptionDescriptor& entry, bool hard_flag) {
    struct rlimit limits{};
    if (!fetch_limits(entry, limits)) {
        return false;
    }

    rlim_t value = hard_flag ? limits.rlim_max : limits.rlim_cur;
    if (value == RLIM_INFINITY) {
        std::cout << "unlimited\n";
    } else {
        rlim_t multiplier = entry.multiplier == 0 ? 1 : entry.multiplier;
        std::cout << (value / multiplier) << '\n';
    }
    return true;
}

std::string unit_for_entry(const OptionDescriptor& entry) {
    if (entry.resource == RLIMIT_CPU) {
        return "seconds";
    }
    if (entry.multiplier == 1) {
        return "count";
    }
    return "kB";
}

bool print_all_limits(bool hard_flag) {
    std::size_t max_width = 0;
    for (const auto& entry : kOptionTable) {
        if (entry.available) {
            max_width = std::max(max_width, std::strlen(entry.description));
        }
    }

    bool success = true;
    for (const auto& entry : kOptionTable) {
        if (!entry.available) {
            continue;
        }

        struct rlimit limits{};
        if (!fetch_limits(entry, limits)) {
            success = false;
            continue;
        }

        rlim_t value = hard_flag ? limits.rlim_max : limits.rlim_cur;
        std::cout << std::left << std::setw(static_cast<int>(max_width)) << entry.description;
        std::cout << " (" << unit_for_entry(entry) << ", -" << entry.short_opt << ") ";
        if (value == RLIM_INFINITY) {
            std::cout << "unlimited\n";
        } else {
            rlim_t multiplier = entry.multiplier == 0 ? 1 : entry.multiplier;
            std::cout << (value / multiplier) << '\n';
        }
    }
    return success;
}

bool set_limit(const OptionDescriptor& entry, bool hard_flag, bool soft_flag, rlim_t value) {
    struct rlimit limits{};
    if (!fetch_limits(entry, limits)) {
        return false;
    }

    struct rlimit new_limits = limits;

    if (hard_flag) {
        new_limits.rlim_max = value;
        if (!soft_flag && value != RLIM_INFINITY && new_limits.rlim_cur > value) {
            new_limits.rlim_cur = value;
        }
    }

    if (soft_flag) {
        if (value == RLIM_INFINITY && new_limits.rlim_max != RLIM_INFINITY) {
            new_limits.rlim_cur = new_limits.rlim_max;
        } else if (new_limits.rlim_max != RLIM_INFINITY && value > new_limits.rlim_max) {
            new_limits.rlim_cur = new_limits.rlim_max;
        } else {
            new_limits.rlim_cur = value;
        }
    }

    if (setrlimit(entry.resource, &new_limits) != 0) {
        std::string message;
        if (errno == EPERM) {
            message = std::string("permission denied when changing resource: ") + entry.description;
        } else {
            message = std::string("setrlimit failed: ") + std::strerror(errno);
        }
        print_error({ErrorType::RUNTIME_ERROR, "ulimit", message, {}});
        return false;
    }

    return true;
}

bool parse_numeric_limit(const std::string& input, const OptionDescriptor& entry, rlim_t& parsed) {
    const char* str = input.c_str();
    if (*str == '\0') {
        return false;
    }
    char* endptr = nullptr;
    errno = 0;
    unsigned long long base = std::strtoull(str, &endptr, 10);
    if (errno != 0 || endptr == nullptr || *endptr != '\0') {
        return false;
    }

    unsigned long long multiplier =
        entry.multiplier == 0 ? 1 : static_cast<unsigned long long>(entry.multiplier);
    if (base > std::numeric_limits<unsigned long long>::max() / multiplier) {
        errno = ERANGE;
        return false;
    }

    unsigned long long product = base * multiplier;
    if (product > static_cast<unsigned long long>(std::numeric_limits<rlim_t>::max())) {
        errno = ERANGE;
        return false;
    }

    parsed = static_cast<rlim_t>(product);
    return true;
}

bool parse_limit_value(const std::string& value_str, const OptionDescriptor& entry,
                       const struct rlimit& limits, rlim_t& result) {
    std::string lowered = to_lower(value_str);
    if (lowered == "unlimited") {
        result = RLIM_INFINITY;
        return true;
    }
    if (lowered == "hard") {
        result = limits.rlim_max;
        return true;
    }
    if (lowered == "soft") {
        result = limits.rlim_cur;
        return true;
    }

    rlim_t numeric_value{};
    if (!parse_numeric_limit(value_str, entry, numeric_value)) {
        return false;
    }

    result = numeric_value;
    return true;
}

int handle_unknown_option(const std::string& option_text) {
    print_error({ErrorType::INVALID_ARGUMENT, "ulimit", "invalid option: " + option_text, {}});
    return 1;
}

int handle_unsupported_option(const OptionDescriptor& entry) {
    print_error(
        {ErrorType::INVALID_ARGUMENT,
         "ulimit",
         std::string("resource limit not available on this operating system: -") + entry.short_opt,
         {}});
    return 1;
}

int print_help_and_exit() {
    for (const auto& line : ulimit_help_text()) {
        std::cout << line << '\n';
    }
    return 0;
}

}  // namespace

int ulimit_command(const std::vector<std::string>& args) {
    if (builtin_handle_help(args, ulimit_help_text())) {
        return 0;
    }

    bool all_flag = false;
    bool hard_flag = false;
    bool soft_flag = false;
    const OptionDescriptor* selected = find_default_descriptor();

    if (selected == nullptr) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "ulimit",
                     "no supported resource limits available on this platform",
                     {}});
        return 1;
    }

    std::size_t idx = 1;
    while (idx < args.size()) {
        const std::string& arg = args[idx];
        if (arg == "--") {
            ++idx;
            break;
        }
        if (arg == "-h" || arg == "--help") {
            return print_help_and_exit();
        }
        if (!arg.empty() && arg[0] == '-' && arg.size() > 1) {
            if (arg[1] == '-') {
                std::string long_opt = arg.substr(2);
                if (long_opt == "all") {
                    all_flag = true;
                } else if (long_opt == "hard") {
                    hard_flag = true;
                } else if (long_opt == "soft") {
                    soft_flag = true;
                } else {
                    const OptionDescriptor* entry = find_by_long_option(long_opt);
                    if (entry == nullptr) {
                        return handle_unknown_option("--" + long_opt);
                    }
                    if (!entry->available) {
                        return handle_unsupported_option(*entry);
                    }
                    selected = entry;
                }
            } else {
                for (std::size_t j = 1; j < arg.size(); ++j) {
                    char opt = arg[j];
                    if (opt == 'a') {
                        all_flag = true;
                    } else if (opt == 'H') {
                        hard_flag = true;
                    } else if (opt == 'S') {
                        soft_flag = true;
                    } else {
                        const OptionDescriptor* entry = find_by_short_option(opt);
                        if (entry == nullptr) {
                            return handle_unknown_option(std::string("-") + opt);
                        }
                        if (!entry->available) {
                            return handle_unsupported_option(*entry);
                        }
                        selected = entry;
                    }
                }
            }
            ++idx;
            continue;
        }
        break;
    }

    if (all_flag) {
        return print_all_limits(hard_flag) ? 0 : 1;
    }

    std::size_t remaining = args.size() - idx;
    if (remaining == 0) {
        return print_limit(*selected, hard_flag) ? 0 : 1;
    }
    if (remaining > 1) {
        print_error({ErrorType::INVALID_ARGUMENT,
                     "ulimit",
                     "too many arguments",
                     {"Provide a single limit value or none."}});
        return 1;
    }

    if (!hard_flag && !soft_flag) {
        hard_flag = true;
        soft_flag = true;
    }

    struct rlimit limits{};
    if (!fetch_limits(*selected, limits)) {
        return 1;
    }

    rlim_t new_value{};
    if (!parse_limit_value(args[idx], *selected, limits, new_value)) {
        std::string message;
        if (errno == ERANGE) {
            message = "limit value out of range";
        } else {
            message = "invalid limit: '" + args[idx] + "'";
        }
        print_error({ErrorType::INVALID_ARGUMENT, "ulimit", message, {}});
        return 1;
    }

    return set_limit(*selected, hard_flag, soft_flag, new_value) ? 0 : 1;
}
