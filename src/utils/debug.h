#pragma once

#ifdef CJSH_ENABLE_DEBUG

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <chrono>
#include <mutex>

#include "cjsh_filesystem.h"

namespace cjsh_debug_detail {
inline std::once_flag g_log_init_flag;
inline FILE* g_log_file = nullptr;
}  // namespace cjsh_debug_detail

static inline int cjsh_debug_enabled(void) {
    const char* value = getenv("CJSH_DEBUG");
    return value != NULL && value[0] == '1' && value[1] == '\0';
}

static inline int cjsh_debug_file_enabled(void) {
    const char* value = getenv("CJSH_DEBUG_FILE");
    return value != NULL && value[0] == '1' && value[1] == '\0';
}

static inline void close_debug_log_file(void) {
    if (cjsh_debug_detail::g_log_file != nullptr) {
        cjsh_filesystem::safe_fclose(cjsh_debug_detail::g_log_file);
        cjsh_debug_detail::g_log_file = nullptr;
    }
}

static inline FILE* cjsh_get_debug_log_file(void) {
    std::call_once(cjsh_debug_detail::g_log_init_flag, []() {
        if (!cjsh_filesystem::initialize_cjsh_directories()) {
            return;
        }

        long long timestamp = static_cast<long long>(time(nullptr));
        char filename[64];
        if (snprintf(filename, sizeof(filename), "cjsh_debug_%lld.log", timestamp) < 0) {
            return;
        }

        auto log_path = cjsh_filesystem::g_cjsh_cache_path / filename;
        auto file_result = cjsh_filesystem::safe_fopen(log_path.string(), "a");
        if (file_result.is_error()) {
            return;
        }

        cjsh_debug_detail::g_log_file = file_result.value();
        atexit(close_debug_log_file);
    });

    return cjsh_debug_detail::g_log_file;
}

static inline void debug_msg(const char* fmt, ...) {
    if (!cjsh_debug_enabled()) {
        return;
    }

    int log_to_file = cjsh_debug_file_enabled();

    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    if (log_to_file) {
        va_copy(args_copy, args);
    }

    fputs("[DEBUG] ", stderr);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    fflush(stderr);

    if (log_to_file) {
        FILE* log_file = cjsh_get_debug_log_file();
        if (log_file != nullptr) {
            fputs("[DEBUG] ", log_file);
            vfprintf(log_file, fmt, args_copy);
            fputc('\n', log_file);
            fflush(log_file);
        }
        va_end(args_copy);
    }
}

class PerformanceTracker {
   public:
    explicit PerformanceTracker(const char* label) : label_(label), enabled_(cjsh_debug_enabled()) {
        if (enabled_) {
            start_time_ = std::chrono::steady_clock::now();
        }
    }

    ~PerformanceTracker() {
        if (!enabled_) {
            return;
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration = end_time - start_time_;
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

        if (microseconds < 1000) {
            debug_msg("PerformanceTracker [%s]: %lld us", label_,
                      static_cast<long long>(microseconds));
        } else {
            double milliseconds = static_cast<double>(microseconds) / 1000.0;
            debug_msg("PerformanceTracker [%s]: %.3f ms", label_, milliseconds);
        }
    }

   private:
    const char* label_;
    bool enabled_{false};
    std::chrono::steady_clock::time_point start_time_{};
};

#else

static inline int cjsh_debug_enabled(void) {
    return 0;
}

static inline int cjsh_debug_file_enabled(void) {
    return 0;
}

static inline void debug_msg(const char* fmt, ...) {
    (void)fmt;
}

class PerformanceTracker {
   public:
    PerformanceTracker(const char* label) {
        (void)label;
    }

    ~PerformanceTracker() {
    }
};

#endif
