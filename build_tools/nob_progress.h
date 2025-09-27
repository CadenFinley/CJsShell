#ifndef CJSH_NOB_PROGRESS_H
#define CJSH_NOB_PROGRESS_H

#include <stdio.h>

#include "nob_platform.h"

static inline void draw_progress_bar(const char* phase, size_t current,
                                     size_t total, size_t width) {
    if (total == 0)
        return;

    float progress = (float)current / (float)total;
    size_t filled = (size_t)(progress * width);

    printf("\r%-20.20s [%s", phase, NOB_ANSI_COLOR_GREEN);

    for (size_t i = 0; i < filled; i++) {
        printf("█");
    }

    printf("%s", NOB_ANSI_COLOR_RESET);

    for (size_t i = filled; i < width; i++) {
        printf("░");
    }

    printf("] %zu/%zu (%.1f%%) ", current, total, progress * 100.0f);
    fflush(stdout);
}

static inline void clear_progress_line(void) {
    printf("\r\033[K");
    fflush(stdout);
}

static inline void update_progress(const char* phase, size_t current,
                                   size_t total) {
    draw_progress_bar(phase, current, total, 40);
    if (current == total) {
        printf("\n");
        fflush(stdout);
    }
}

#endif  // CJSH_NOB_PROGRESS_H
