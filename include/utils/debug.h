#pragma once

#ifdef CJSH_ENABLE_DEBUG

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static inline int cjsh_debug_enabled(void) {
    const char* value = getenv("CJSH_DEBUG");
    return value != NULL && value[0] == '1' && value[1] == '\0';
}

static inline void debug_msg(const char* fmt, ...) {
    if (!cjsh_debug_enabled()) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    fputs("[DEBUG] ", stderr);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    fflush(stderr);
}

#else

static inline int cjsh_debug_enabled(void) {
    return 0;
}

static inline void debug_msg(const char* fmt, ...) {
    (void)fmt;
}

#endif
