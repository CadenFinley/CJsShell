#include "libintl_shim.h"

#include <cstdarg>
#include <cstdio>

// Implementation of libintl_snprintf that simply forwards to standard snprintf
extern "C" int libintl_snprintf(char* str, size_t size, const char* format,
                                ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

// Implementation of libintl_printf that simply forwards to standard printf
extern "C" int libintl_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}
