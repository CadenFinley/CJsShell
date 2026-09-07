/*
  lifecycle_probe.c

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

/* Native process probes for shell lifecycle regression tests. */

#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int log_fd = -1;
static int exit_on_hup = 0;

static void record_hup(int signum) {
    (void)signum;
    (void)write(log_fd, "HUP\n", 4);
    if (exit_on_hup) {
        _exit(0);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        return 2;
    }
    log_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (log_fd < 0) {
        return 3;
    }
    if (strcmp(argv[1], "signals") == 0) {
        const int signals[] = {SIGHUP, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGPIPE};
        const char* names[] = {"HUP", "QUIT", "TSTP", "TTIN", "TTOU", "PIPE"};
        sigset_t mask;
        (void)sigprocmask(SIG_SETMASK, NULL, &mask);
        for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
            struct sigaction action;
            (void)sigaction(signals[i], NULL, &action);
            dprintf(log_fd, "%s:%s:%d\n", names[i],
                    action.sa_handler == SIG_IGN   ? "ignored"
                    : action.sa_handler == SIG_DFL ? "default"
                                                   : "caught",
                    sigismember(&mask, signals[i]));
        }
        return 0;
    }
    if (strcmp(argv[1], "foreground") == 0) {
        int tty = open("/dev/tty", O_RDWR);
        if (tty < 0) {
            return 4;
        }
        dprintf(log_fd, "%d\n", tcgetpgrp(tty) == getpgrp());
        return 0;
    }
    if (strcmp(argv[1], "ignore") == 0) {
        (void)signal(SIGHUP, SIG_IGN);
    } else if (strcmp(argv[1], "default") != 0) {
        exit_on_hup = strcmp(argv[1], "exit") == 0;
        (void)signal(SIGHUP, record_hup);
    }
    dprintf(log_fd, "%ld\n", (long)getpid());
    if (strcmp(argv[1], "stop") == 0) {
        (void)kill(getpid(), SIGSTOP);
        (void)write(log_fd, "CONT\n", 5);
    }
    for (;;) {
        pause();
    }
}
