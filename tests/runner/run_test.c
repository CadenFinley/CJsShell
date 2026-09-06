/*
  run_test.c

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

/* Run a test without access to the caller's controlling terminal. */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t child_pid = -1;

static void forward_signal(int signum) {
    const int saved_errno = errno;
    const pid_t pid = (pid_t)child_pid;
    if (pid > 0) {
        if (kill(-pid, signum) < 0 && errno == ESRCH) {
            /* The child may not have reached setsid yet. */
            (void)kill(pid, signum);
        }
        (void)kill(-pid, SIGCONT);
    }
    errno = saved_errno;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: %s command [arguments...]\n", argv[0]);
        return 2;
    }

    const int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT};
    sigset_t blocked, original_mask;
    sigemptyset(&blocked);
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        sigaddset(&blocked, signals[i]);
    }
    if (sigprocmask(SIG_BLOCK, &blocked, &original_mask) < 0) {
        perror("test runner: sigprocmask");
        return 1;
    }

    /* Fork even if the launcher is a process-group leader (as it is when
       launched by an interactive shell), so setsid always has a fresh child. */
    const pid_t pid = fork();
    if (pid < 0) {
        perror("test runner: fork");
        return 1;
    }
    if (pid == 0) {
        if (setsid() < 0) {
            perror("test runner: setsid");
            _exit(1);
        }
        const int input = open("/dev/null", O_RDONLY);
        if (input < 0 || dup2(input, STDIN_FILENO) < 0) {
            perror("test runner: stdin");
            _exit(1);
        }
        if (input != STDIN_FILENO) {
            (void)close(input);
        }
        for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
            (void)signal(signals[i], SIG_DFL);
        }
        (void)sigprocmask(SIG_SETMASK, &original_mask, NULL);
        execvp(argv[1], argv + 1);
        perror("test runner: exec");
        _exit(127);
    }

    child_pid = pid;
    struct sigaction action = {0};
    action.sa_handler = forward_signal;
    sigemptyset(&action.sa_mask);
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        (void)sigaction(signals[i], &action, NULL);
    }
    (void)sigprocmask(SIG_SETMASK, &original_mask, NULL);

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("test runner: waitpid");
            return 1;
        }
    }
    child_pid = -1;
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return WEXITSTATUS(status);
}
