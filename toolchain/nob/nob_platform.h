#ifndef CJSH_NOB_PLATFORM_H
#define CJSH_NOB_PLATFORM_H

#if defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64
#elif defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || \
    defined(_M_X64)
#define ARCH_X86_64
#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
#define ARCH_X86
#else
#define ARCH_UNKNOWN
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_MACOS
#elif defined(__linux__)
#define PLATFORM_LINUX
#elif defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#else
#define PLATFORM_UNIX
#endif

#endif
