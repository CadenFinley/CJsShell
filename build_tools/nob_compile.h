#ifndef CJSH_NOB_COMPILE_H
#define CJSH_NOB_COMPILE_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "nob_dependencies.h"
#include "nob_progress.h"
#include "nob_sources.h"
#include "nob_toolchain.h"

static inline bool compile_cjsh(void) {
    nob_log(NOB_INFO, "Compiling " PROJECT_NAME " (parallel compilation)...");

    String_Array cpp_sources = {0};
    String_Array c_sources = {0};
    String_Array obj_files = {0};

    if (!collect_sources(&cpp_sources)) {
        return false;
    }

    if (!collect_c_sources(&c_sources)) {
        return false;
    }

    Nob_Procs procs = {0};
    int max_parallel_jobs = nob_nprocs();
    if (max_parallel_jobs <= 0)
        max_parallel_jobs = 4;

    size_t total_files = cpp_sources.count + c_sources.count;
    (void)total_files;

    nob_log(NOB_INFO, "Using %d parallel compilation jobs", max_parallel_jobs);

    Nob_Log_Level original_log_level = nob_minimal_log_level;
    size_t completed_cpp_files = 0;

    nob_log(NOB_INFO, "Starting parallel compilation of %zu C++ files...",
            cpp_sources.count);
    nob_minimal_log_level = NOB_WARNING;
    for (size_t i = 0; i < cpp_sources.count; i++) {
        Nob_Cmd cmd = {0};
        if (!setup_build_flags(&cmd)) {
            nob_minimal_log_level = original_log_level;
            return false;
        }

        nob_cmd_append(&cmd, "-c");
        nob_cmd_append(&cmd, cpp_sources.items[i]);

        const char* source = cpp_sources.items[i];
        const char* basename = strrchr(source, '/');
        if (basename)
            basename++;
        else
            basename = source;

        Nob_String_Builder obj_name = {0};
        nob_sb_append_cstr(&obj_name, "build/obj/");
        size_t base_len = strlen(basename);
        if (base_len > 4 && strcmp(basename + base_len - 4, ".cpp") == 0) {
            nob_sb_append_buf(&obj_name, basename, base_len - 4);
            nob_sb_append_cstr(&obj_name, ".o");
        } else {
            nob_sb_append_cstr(&obj_name, basename);
            nob_sb_append_cstr(&obj_name, ".o");
        }
        nob_sb_append_null(&obj_name);

        nob_cmd_append(&cmd, "-o", obj_name.items);
        nob_da_append(&obj_files, strdup(obj_name.items));

        if (!nob_cmd_run(&cmd, .async = &procs,
                         .max_procs = max_parallel_jobs)) {
            nob_minimal_log_level = original_log_level;
            nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
            nob_sb_free(obj_name);
            return false;
        }

        const char* progress_label = (i + 1 == cpp_sources.count) ? "Complete!" : basename;
        update_progress(progress_label, i + 1, cpp_sources.count);
        nob_sb_free(obj_name);
        completed_cpp_files++;
    }

    nob_minimal_log_level = original_log_level;
    nob_log(NOB_INFO, "Waiting for C++ compilation to complete...");
    nob_minimal_log_level = NOB_WARNING;
    if (!nob_procs_flush(&procs)) {
        clear_progress_line();
        nob_minimal_log_level = original_log_level;
        nob_log(NOB_ERROR, "C++ compilation failed");
        return false;
    }
    clear_progress_line();
    nob_minimal_log_level = original_log_level;
    nob_log(NOB_INFO, "All %zu C++ files compiled successfully",
            cpp_sources.count);

    nob_log(NOB_INFO, "Starting parallel compilation of %zu C files...",
            c_sources.count);
    nob_minimal_log_level = NOB_WARNING;
    for (size_t i = 0; i < c_sources.count; i++) {
        Nob_Cmd cmd = {0};
        if (!setup_c_build_flags(&cmd)) {
            nob_minimal_log_level = original_log_level;
            return false;
        }

        nob_cmd_append(&cmd, "-c");
        nob_cmd_append(&cmd, c_sources.items[i]);

        const char* source = c_sources.items[i];
        const char* basename = strrchr(source, '/');
        if (basename)
            basename++;
        else
            basename = source;

        Nob_String_Builder obj_name = {0};
        nob_sb_append_cstr(&obj_name, "build/obj/");
        size_t base_len = strlen(basename);
        if (base_len > 2 && strcmp(basename + base_len - 2, ".c") == 0) {
            nob_sb_append_buf(&obj_name, basename, base_len - 2);
            nob_sb_append_cstr(&obj_name, ".c.o");
        } else {
            nob_sb_append_cstr(&obj_name, basename);
            nob_sb_append_cstr(&obj_name, ".c.o");
        }
        nob_sb_append_null(&obj_name);

        nob_cmd_append(&cmd, "-o", obj_name.items);
        nob_da_append(&obj_files, strdup(obj_name.items));

        if (!nob_cmd_run(&cmd, .async = &procs,
                         .max_procs = max_parallel_jobs)) {
            nob_minimal_log_level = original_log_level;
            nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
            nob_sb_free(obj_name);
            return false;
        }

        const char* progress_label = (i + 1 == c_sources.count) ? "Complete!" : basename;
        update_progress(progress_label, i + 1, c_sources.count);
        nob_sb_free(obj_name);
    }

    nob_minimal_log_level = original_log_level;
    nob_log(NOB_INFO, "Waiting for C compilation to complete...");
    nob_minimal_log_level = NOB_WARNING;
    if (!nob_procs_flush(&procs)) {
        clear_progress_line();
        nob_minimal_log_level = original_log_level;
        nob_log(NOB_ERROR, "C compilation failed");
        return false;
    }
    clear_progress_line();
    nob_minimal_log_level = original_log_level;
    nob_log(NOB_INFO, "All %zu files compiled successfully!",
            cpp_sources.count + c_sources.count);

    nob_minimal_log_level = NOB_WARNING;
    Nob_Cmd link_cmd = {0};

    const char* linker = get_linker();
    nob_cmd_append(&link_cmd, linker);

#ifdef PLATFORM_MACOS
    if (strcmp(linker, "clang++") == 0) {
        nob_cmd_append(&link_cmd, "-stdlib=libc++");
    }
#ifdef ARCH_ARM64
    nob_cmd_append(&link_cmd, "-arch", "arm64");
#elif defined(ARCH_X86_64)
    nob_cmd_append(&link_cmd, "-arch", "x86_64");
#endif
#endif

#ifdef PLATFORM_LINUX
    if (strcmp(linker, "g++") == 0) {
        nob_cmd_append(&link_cmd, "-static-libgcc", "-static-libstdc++");
    }
#endif

    for (size_t i = 0; i < obj_files.count; i++) {
        nob_cmd_append(&link_cmd, obj_files.items[i]);
    }

    nob_cmd_append(&link_cmd, "-o", "build/" PROJECT_NAME);

#ifdef PLATFORM_MACOS
    if (strcmp(linker, "clang++") == 0) {
        nob_cmd_append(&link_cmd, "-lpthread");
    } else {
        nob_cmd_append(&link_cmd, "-lstdc++", "-lpthread");
    }
#else
    nob_cmd_append(&link_cmd, "-lstdc++", "-lpthread");
#endif

#if defined(PLATFORM_LINUX) || defined(PLATFORM_UNIX)
    nob_cmd_append(&link_cmd, "-ldl");
#endif

    for (size_t i = 0; i < build_config.external_library_paths_count; i++) {
        nob_cmd_append(&link_cmd, build_config.external_library_paths[i]);
    }

    if (!nob_cmd_run(&link_cmd)) {
        nob_minimal_log_level = original_log_level;
        nob_log(NOB_ERROR, "Linking failed");
        return false;
    }

    nob_minimal_log_level = original_log_level;

    nob_da_free(cpp_sources);
    nob_da_free(c_sources);
    for (size_t i = 0; i < obj_files.count; i++) {
        (void)obj_files.items[i];
    }
    nob_da_free(obj_files);
    nob_da_free(procs);

    return true;
}

#endif  // CJSH_NOB_COMPILE_H
