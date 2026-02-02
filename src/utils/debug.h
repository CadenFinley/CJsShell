/*
  debug.h

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
        (void)fclose(cjsh_debug_detail::g_log_file);
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

        auto log_path = cjsh_filesystem::g_cjsh_cache_path() / filename;
        cjsh_debug_detail::g_log_file = fopen(log_path.string().c_str(), "a");
        if (cjsh_debug_detail::g_log_file == nullptr) {
            return;
        }

        (void)atexit(close_debug_log_file);
    });

    return cjsh_debug_detail::g_log_file;
}

static inline void cjsh_debug_msg(const char* fmt, ...) {
    if (!cjsh_debug_enabled()) {
        return;
    }

    FILE* output_stream = nullptr;
    if (cjsh_debug_file_enabled()) {
        output_stream = cjsh_get_debug_log_file();
    }

    if (output_stream == nullptr) {
        output_stream = stderr;
    }

    va_list args;
    va_start(args, fmt);

    (void)fputs("[DEBUG] ", output_stream);
    (void)vfprintf(output_stream, fmt, args);
    va_end(args);
    (void)fputc('\n', output_stream);
    (void)fflush(output_stream);
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
            cjsh_debug_msg("PerformanceTracker [%s]: %lld us", label_,
                           static_cast<long long>(microseconds));
        } else {
            double milliseconds = static_cast<double>(microseconds) / 1000.0;
            cjsh_debug_msg("PerformanceTracker [%s]: %.3f ms", label_, milliseconds);
        }
    }

   private:
    const char* label_;
    bool enabled_{false};
    std::chrono::steady_clock::time_point start_time_;
};

#else

static inline int cjsh_debug_enabled(void) {
    return 0;
}

static inline int cjsh_debug_file_enabled(void) {
    return 0;
}

static inline void cjsh_debug_msg(const char* fmt, ...) {
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
