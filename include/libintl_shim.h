#ifndef LIBINTL_SHIM_H
#define LIBINTL_SHIM_H

#include <cstdarg>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

// This function is needed to satisfy the linker
int libintl_snprintf(char* str, size_t size, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif  // LIBINTL_SHIM_H
