#include "string_pool.h"

// Thread-local string pool instance
thread_local FastStringPool g_string_pool;