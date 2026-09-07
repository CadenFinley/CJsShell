/* Native process probes for shell lifecycle regression tests. MIT License. */
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
