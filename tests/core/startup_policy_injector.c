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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static uid_t injected_geteuid(void) {
    return getuid() + (getenv("CJSH_TEST_UID_MISMATCH") != NULL ? 1 : 0);
}
static gid_t injected_getegid(void) {
    return getgid() + (getenv("CJSH_TEST_GID_MISMATCH") != NULL ? 1 : 0);
}
static FILE* injected_fopen(const char* path, const char* mode) {
    const char* profile = getenv("CJSH_TEST_SYSTEM_PROFILE");
    if (profile != NULL && strcmp(path, "/etc/profile") == 0)
        path = profile;
    int flags = mode[0] == 'r' ? O_RDONLY : O_WRONLY | O_CREAT;
    if (mode[0] == 'w')
        flags |= O_TRUNC;
    if (mode[0] == 'a')
        flags |= O_APPEND;
    if (strchr(mode, '+') != NULL)
        flags = (flags & ~O_ACCMODE) | O_RDWR;
    if (strchr(mode, 'x') != NULL)
        flags |= O_EXCL;
    if (strchr(mode, 'e') != NULL)
        flags |= O_CLOEXEC;
    int fd = open(path, flags, 0666);
    if (fd < 0)
        return NULL;
    FILE* stream = fdopen(fd, mode);
    if (stream == NULL) {
        int saved = errno;
        close(fd);
        errno = saved;
    }
    return stream;
}
#if defined(__APPLE__)
#define INTERPOSE(replacement, replacee)                                      \
    __attribute__((used)) static struct {                                     \
        const void* a;                                                        \
        const void* b;                                                        \
    } interpose_##replacee __attribute__((section("__DATA,__interpose"))) = { \
        (const void*)(unsigned long)&replacement, (const void*)(unsigned long)&replacee};
INTERPOSE(injected_geteuid, geteuid)
INTERPOSE(injected_getegid, getegid)
INTERPOSE(injected_fopen, fopen)
#else
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
