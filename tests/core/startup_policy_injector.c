/*
  startup_policy_injector.c

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

// Isolated startup-policy coverage without changing credentials or /etc/profile.
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static uid_t injected_geteuid(void) {
    return getuid() + (getenv("CJSH_TEST_UID_MISMATCH") != NULL ? 1 : 0);
}
static gid_t injected_getegid(void) {
    return getgid() + (getenv("CJSH_TEST_GID_MISMATCH") != NULL ? 1 : 0);
}
static const char* prepare_open_path(const char* path) {
    const char* profile = getenv("CJSH_TEST_SYSTEM_PROFILE");
    if (profile != NULL && strcmp(path, "/etc/profile") == 0) {
        path = profile;
    }
    static int swapped = 0;
    const char* swap_path = getenv("CJSH_TEST_STARTUP_SWAP_PATH");
    if (!swapped && swap_path != NULL && strcmp(path, swap_path) == 0) {
        swapped = 1;
        if (unlink(path) != 0 || mkfifo(path, 0600) != 0) {
            _exit(125);
        }
    }
    return path;
}
static int open_with_arguments(const char* path, int flags, va_list args) {
    mode_t mode = 0;
    if ((flags & O_CREAT) != 0
#ifdef O_TMPFILE
        || (flags & O_TMPFILE) == O_TMPFILE
#endif
    )
        mode = (mode_t)va_arg(args, int);
    return openat(AT_FDCWD, prepare_open_path(path), flags, mode);
}
static FILE* injected_fopen(const char* path, const char* mode) {
    int flags = mode[0] == 'r' ? O_RDONLY : O_WRONLY | O_CREAT;
    if (mode[0] == 'w') {
        flags |= O_TRUNC;
    }
    if (mode[0] == 'a') {
        flags |= O_APPEND;
    }
    if (strchr(mode, '+') != NULL) {
        flags = (flags & ~O_ACCMODE) | O_RDWR;
    }
    if (strchr(mode, 'x') != NULL) {
        flags |= O_EXCL;
    }
    if (strchr(mode, 'e') != NULL) {
        flags |= O_CLOEXEC;
    }
    int fd = openat(AT_FDCWD, prepare_open_path(path), flags, 0666);
    if (fd < 0) {
        return NULL;
    }
    FILE* stream = fdopen(fd, mode);
    if (stream == NULL) {
        int saved = errno;
        close(fd);
        errno = saved;
    }
    return stream;
}
#if defined(__APPLE__)
static int injected_open(const char* path, int flags, ...) {
    va_list args;
    va_start(args, flags);
    int fd = open_with_arguments(path, flags, args);
    va_end(args);
    return fd;
}
#define INTERPOSE(replacement, replacee)                                      \
    __attribute__((used)) static struct {                                     \
        const void* a;                                                        \
        const void* b;                                                        \
    } interpose_##replacee __attribute__((section("__DATA,__interpose"))) = { \
        (const void*)(unsigned long)&replacement, (const void*)(unsigned long)&replacee};
INTERPOSE(injected_geteuid, geteuid)
INTERPOSE(injected_getegid, getegid)
INTERPOSE(injected_fopen, fopen)
INTERPOSE(injected_open, open)
#else
int open(const char* path, int flags, ...) {
    va_list args;
    va_start(args, flags);
    int fd = open_with_arguments(path, flags, args);
    va_end(args);
    return fd;
}
int open64(const char* path, int flags, ...) {
    va_list args;
    va_start(args, flags);
    int fd = open_with_arguments(path, flags, args);
    va_end(args);
    return fd;
}
uid_t geteuid(void) {
    return injected_geteuid();
}
gid_t getegid(void) {
    return injected_getegid();
}
FILE* fopen64(const char* path, const char* mode) {
    return injected_fopen(path, mode);
}
FILE* fopen(const char* path, const char* mode) {
    return injected_fopen(path, mode);
}
#endif
