#ifndef CJSH_NOB_PROGRESS_H
#define CJSH_NOB_PROGRESS_H

#include <stdio.h>

#ifdef PLATFORM_WINDOWS
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "nob_platform.h"

// Global flag to track if we should use progress bars
static int nob_use_progress_bars = -1;

// Check if stdout is a TTY and we should show progress bars
static inline int should_show_progress(void) {
    if (nob_use_progress_bars == -1) {
        nob_use_progress_bars = isatty(fileno(stdout)) ? 1 : 0;
    }
    return nob_use_progress_bars;
}

static inline void draw_progress_bar(const char* phase, size_t current, size_t total,
                                     size_t width) {
    if (total == 0 || !should_show_progress())
        return;

    float progress = (float)current / (float)total;
    size_t filled = (size_t)(progress * width);

    // Use more robust clearing and positioning
    printf("\r\033[K%-20.20s [%s", phase, NOB_ANSI_COLOR_GREEN);

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
    if (!should_show_progress())
        return;

    // More robust clearing - clear line and move cursor to beginning
    printf("\r\033[K\r");
    fflush(stdout);
}

static inline void update_progress(const char* phase, size_t current, size_t total) {
    if (should_show_progress()) {
        draw_progress_bar(phase, current, total, 40);
    } else {
        // Fallback for non-TTY: show simple progress indicators
        if (current == total) {
            printf("Complete: %s (%zu/%zu)\n", phase, current, total);
        } else if (current % 5 == 0 || current == 1) {
            // Only show progress every 5 files to avoid spam
            printf("Progress: %s (%zu/%zu)\n", phase, current, total);
        }
    }
    fflush(stdout);
}

// Function to handle compiler output interruptions
static inline void handle_compiler_output_interruption(void) {
    if (should_show_progress()) {
        // Clear any partial progress line that might be corrupted
        printf("\r\033[K");
        fflush(stdout);
    }
}

// Enhanced progress update that handles interruptions better
static inline void update_progress_safe(const char* phase, size_t current, size_t total) {
    if (should_show_progress()) {
        // Clear any existing content first
        printf("\r\033[K");
        draw_progress_bar(phase, current, total, 40);
    } else {
        // Fallback for non-TTY
        if (current == total) {
            printf("Complete: %s (%zu/%zu)\n", phase, current, total);
        } else if (current % 5 == 0 || current == 1) {
            printf("Progress: %s (%zu/%zu)\n", phase, current, total);
        }
    }
    fflush(stdout);
}

#endif  // CJSH_NOB_PROGRESS_H
