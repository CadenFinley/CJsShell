/*
  fg_terminal_race_injector.c

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

  This test-only tcsetpgrp interposer terminates a selected stopped process group immediately
  before fg hands it the terminal, then returns EINVAL so the shell must refresh the child status
  and recover from the race.
*/

#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t injection_fired = 0;

static pid_t read_target_pgid(const char* path) {
    char buffer[32];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    ssize_t length = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (length <= 0) {
        return -1;
    }

    buffer[length] = '\0';
    char* end = NULL;
    long parsed = strtol(buffer, &end, 10);
    if (end == buffer || parsed <= 0) {
        return -1;
    }
    return (pid_t)parsed;
}

static void wait_until_child_status_is_ready(pid_t pid) {
    const struct timespec retry_delay = {0, 1000000};

    for (int attempt = 0; attempt < 2000; ++attempt) {
        siginfo_t info;
        info.si_pid = 0;
        if (waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG | WNOWAIT) == 0 &&
            info.si_pid == pid) {
            return;
        }
        nanosleep(&retry_delay, NULL);
    }
}

static void record_injection(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return;
    }
    (void)write(fd, "triggered\n", 10);
    close(fd);
}

static int injected_tcsetpgrp(int fd, pid_t pgrp) {
    const char* target_path = getenv("CJSH_TEST_FG_RACE_TARGET_FILE");
    const char* arm_path = getenv("CJSH_TEST_FG_RACE_ARM_FILE");
    const char* result_path = getenv("CJSH_TEST_FG_RACE_RESULT_FILE");

    if (!injection_fired && target_path != NULL && arm_path != NULL && result_path != NULL &&
        access(arm_path, F_OK) == 0 && read_target_pgid(target_path) == pgrp) {
        injection_fired = 1;

        (void)kill(-pgrp, SIGTERM);
        (void)kill(-pgrp, SIGCONT);
        wait_until_child_status_is_ready(pgrp);
        record_injection(result_path);

        errno = EINVAL;
        return -1;
    }

    return ioctl(fd, TIOCSPGRP, &pgrp);
}

#if defined(__APPLE__)
#define DYLD_INTERPOSE(replacement, replacee)                                  \
    __attribute__((used)) static struct {                                      \
        const void* replacement;                                               \
        const void* replacee;                                                  \
    } _interpose_##replacee __attribute__((section("__DATA,__interpose"))) = { \
        (const void*)(unsigned long)&replacement, (const void*)(unsigned long)&replacee};

DYLD_INTERPOSE(injected_tcsetpgrp, tcsetpgrp)
#else
int tcsetpgrp(int fd, pid_t pgrp) {
    return injected_tcsetpgrp(fd, pgrp);
}
#endif
