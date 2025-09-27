#ifndef CJSH_NOB_TYPES_H
#define CJSH_NOB_TYPES_H

#include <stddef.h>

typedef struct {
    const char** items;
    size_t count;
    size_t capacity;
} String_Array;

typedef struct {
    const char** main_sources;
    size_t main_sources_count;

    const char** module_directories;
    size_t module_directories_count;

    const char** isocline_c_sources;
    size_t isocline_c_sources_count;

    const char** include_directories;
    size_t include_directories_count;

    const char** c_include_directories;
    size_t c_include_directories_count;

    const char** required_directories;
    size_t required_directories_count;

    const char** external_dependencies;
    size_t external_dependencies_count;

    const char** external_library_paths;
    size_t external_library_paths_count;

    const char** dependency_urls;
    size_t dependency_urls_count;
} Build_Config;

#endif  // CJSH_NOB_TYPES_H
