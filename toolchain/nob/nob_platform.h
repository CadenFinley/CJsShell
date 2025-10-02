#ifndef CJSH_NOB_PLATFORM_H
#define CJSH_NOB_PLATFORM_H

// ANSI color codes for progress bar
#define NOB_ANSI_COLOR_GREEN "\033[32m"
#define NOB_ANSI_COLOR_YELLOW "\033[33m"
#define NOB_ANSI_COLOR_RED "\033[31m"
#define NOB_ANSI_COLOR_RESET "\033[0m"

// Architecture detection
#if defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64
#elif defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_X64)
#define ARCH_X86_64
#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
#define ARCH_X86
#else
#define ARCH_UNKNOWN
#endif

// Platform detection
#if defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_MACOS
#elif defined(__linux__)
#define PLATFORM_LINUX
#elif defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#else
#define PLATFORM_UNIX
#endif

#endif  // CJSH_NOB_PLATFORM_H
