#ifndef CJSH_NOB_SOURCES_H
#define CJSH_NOB_SOURCES_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if !defined(NOB_IMPLEMENTATION)
#include "nob.h"
#endif
#include "nob_build_config.h"

static inline bool nob_path_has_extension(const char* path, const char* extension) {
    if (path == NULL || extension == NULL) {
        return false;
    }

    size_t path_len = strlen(path);
    size_t ext_len = strlen(extension);
    if (path_len < ext_len) {
        return false;
    }

    return strcmp(path + path_len - ext_len, extension) == 0;
}

static inline bool nob_is_header_file(const char* path) {
    static const char* header_exts[] = {".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".tpp"};

    for (size_t i = 0; i < sizeof(header_exts) / sizeof(header_exts[0]); i++) {
        if (nob_path_has_extension(path, header_exts[i])) {
            return true;
        }
    }

    return false;
}

static inline bool collect_sources(String_Array* sources) {
    nob_log(NOB_INFO, "Collecting source files...");

    for (size_t i = 0; i < build_config.main_sources_count; i++) {
        const char* path = build_config.main_sources[i];
        if (nob_get_file_type(path) == NOB_FILE_REGULAR) {
            if (nob_is_header_file(path)) {
                nob_log(NOB_WARNING,
                        "Skipping header-only file listed in main_sources: %s (no compilation will "
                        "be performed)",
                        path);
                continue;
            }

            nob_da_append(sources, path);
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
        const char* path = build_config.isocline_c_sources[i];
        if (nob_get_file_type(path) == NOB_FILE_REGULAR) {
            if (nob_is_header_file(path)) {
                nob_log(NOB_WARNING,
                        "Skipping header-only file listed in isocline_c_sources: %s (no "
                        "compilation will be performed)",
                        path);
                continue;
            }

            nob_da_append(c_sources, path);
        }
    }

    nob_log(NOB_INFO, "Collected %zu C source files", c_sources->count);
    return true;
}

#endif
