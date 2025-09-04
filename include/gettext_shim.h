#ifndef GETTEXT_SHIM_H
#define GETTEXT_SHIM_H

#include <cstdio>
#include <cstdarg>

#ifdef __cplusplus
extern "C" {
#endif

// Define libintl_snprintf if it's not already defined
#ifndef libintl_snprintf
#define libintl_snprintf snprintf
#endif

#ifdef __cplusplus
}
#endif

#endif // GETTEXT_SHIM_H
