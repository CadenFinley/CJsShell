#ifndef CJSH_NOB_SOURCES_H
#define CJSH_NOB_SOURCES_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "nob_build_config.h"

static inline bool collect_sources(String_Array* sources) {
    nob_log(NOB_INFO, "Collecting source files...");

    for (size_t i = 0; i < build_config.main_sources_count; i++) {
        if (nob_get_file_type(build_config.main_sources[i]) ==
            NOB_FILE_REGULAR) {
            nob_da_append(sources, build_config.main_sources[i]);
        }
    }

    for (size_t d = 0; d < build_config.module_directories_count; d++) {
        const char* module_dir = build_config.module_directories[d];
        Nob_File_Paths module_files = {0};

        if (nob_read_entire_dir(module_dir, &module_files)) {
            for (size_t i = 0; i < module_files.count; i++) {
                const char* file = module_files.items[i];
                size_t len = strlen(file);
                if (len > 4 && strcmp(file + len - 4, ".cpp") == 0) {
                    if (strcmp(module_dir, "src/utils") == 0 &&
                        strcmp(file, "libintl_shim.cpp") == 0) {
                        continue;
                    }

                    Nob_String_Builder sb = {0};
                    nob_sb_append_cstr(&sb, module_dir);
                    nob_sb_append_cstr(&sb, "/");
                    nob_sb_append_cstr(&sb, file);
                    nob_sb_append_null(&sb);
                    nob_da_append(sources, strdup(sb.items));
                    nob_sb_free(sb);
                }
            }
            nob_da_free(module_files);
        }
    }

    nob_log(NOB_INFO, "Collected %zu C++ source files", sources->count);
    return true;
}

static inline bool collect_c_sources(String_Array* c_sources) {
    nob_log(NOB_INFO, "Collecting C source files...");

    for (size_t i = 0; i < build_config.isocline_c_sources_count; i++) {
        if (nob_get_file_type(build_config.isocline_c_sources[i]) ==
            NOB_FILE_REGULAR) {
            nob_da_append(c_sources, build_config.isocline_c_sources[i]);
        }
    }

    nob_log(NOB_INFO, "Collected %zu C source files", c_sources->count);
    return true;
}

#endif  // CJSH_NOB_SOURCES_H
