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
    nob_log(NOB_INFO, "Compiling " PROJECT_NAME "...");

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

    size_t total_source_files = cpp_sources.count + c_sources.count;
    (void)total_source_files;

    nob_log(NOB_INFO, "Using %d parallel compilation jobs", max_parallel_jobs);

    Nob_Log_Level original_log_level = nob_minimal_log_level;
    size_t completed_cpp_files = 0;

    // First pass: determine which files need to be compiled
    String_Array files_to_compile = {0};
    String_Array corresponding_obj_files = {0};

    for (size_t i = 0; i < cpp_sources.count; i++) {
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

        // Check if compilation is needed
        int rebuild_result = nob_needs_rebuild1(obj_name.items, source);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if %s needs rebuild", source);
            nob_sb_free(obj_name);
            return false;
        }

        if (rebuild_result > 0) {
            // File needs to be compiled
            nob_da_append(&files_to_compile, source);
            nob_da_append(&corresponding_obj_files, strdup(obj_name.items));
        }

        // Always add to obj_files for linking
        nob_da_append(&obj_files, strdup(obj_name.items));
        nob_sb_free(obj_name);
    }

    if (files_to_compile.count == 0) {
        nob_log(NOB_INFO, "All C++ files are up to date, skipping compilation");
    } else {
        nob_log(NOB_INFO,
                "Starting parallel compilation of %zu C++ files (skipping %zu "
                "up-to-date)...",
                files_to_compile.count,
                cpp_sources.count - files_to_compile.count);
        nob_minimal_log_level = NOB_WARNING;
        for (size_t i = 0; i < files_to_compile.count; i++) {
            Nob_Cmd cmd = {0};
            if (!setup_build_flags(&cmd)) {
                nob_minimal_log_level = original_log_level;
                return false;
            }

            nob_cmd_append(&cmd, "-c");
            nob_cmd_append(&cmd, files_to_compile.items[i]);

            const char* source = files_to_compile.items[i];
            const char* basename = strrchr(source, '/');
            if (basename)
                basename++;
            else
                basename = source;

            nob_cmd_append(&cmd, "-o", corresponding_obj_files.items[i]);

            if (!nob_cmd_run(&cmd, .async = &procs,
                             .max_procs = max_parallel_jobs)) {
                nob_minimal_log_level = original_log_level;
                nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
                return false;
            }

            const char* progress_label =
                (i + 1 == files_to_compile.count) ? "Complete!" : basename;
            update_progress(progress_label, i + 1, files_to_compile.count);
            completed_cpp_files++;
        }
    }

    nob_minimal_log_level = original_log_level;
    if (files_to_compile.count > 0) {
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
                files_to_compile.count);
    }

    // Clean up temporary arrays and track totals
    size_t cpp_files_compiled = files_to_compile.count;
    nob_da_free(files_to_compile);
    for (size_t i = 0; i < corresponding_obj_files.count; i++) {
        free((char*)corresponding_obj_files.items[i]);
    }
    nob_da_free(corresponding_obj_files);

    // Second pass: determine which C files need to be compiled
    String_Array c_files_to_compile = {0};
    String_Array c_corresponding_obj_files = {0};

    for (size_t i = 0; i < c_sources.count; i++) {
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

        // Check if compilation is needed
        int rebuild_result = nob_needs_rebuild1(obj_name.items, source);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if %s needs rebuild", source);
            nob_sb_free(obj_name);
            return false;
        }

        if (rebuild_result > 0) {
            // File needs to be compiled
            nob_da_append(&c_files_to_compile, source);
            nob_da_append(&c_corresponding_obj_files, strdup(obj_name.items));
        }

        // Always add to obj_files for linking
        nob_da_append(&obj_files, strdup(obj_name.items));
        nob_sb_free(obj_name);
    }

    if (c_files_to_compile.count == 0) {
        nob_log(NOB_INFO, "All C files are up to date, skipping compilation");
    } else {
        nob_log(NOB_INFO,
                "Starting parallel compilation of %zu C files (skipping %zu "
                "up-to-date)...",
                c_files_to_compile.count,
                c_sources.count - c_files_to_compile.count);
        nob_minimal_log_level = NOB_WARNING;
        for (size_t i = 0; i < c_files_to_compile.count; i++) {
            Nob_Cmd cmd = {0};
            if (!setup_c_build_flags(&cmd)) {
                nob_minimal_log_level = original_log_level;
                return false;
            }

            nob_cmd_append(&cmd, "-c");
            nob_cmd_append(&cmd, c_files_to_compile.items[i]);

            const char* source = c_files_to_compile.items[i];
            const char* basename = strrchr(source, '/');
            if (basename)
                basename++;
            else
                basename = source;

            nob_cmd_append(&cmd, "-o", c_corresponding_obj_files.items[i]);

            if (!nob_cmd_run(&cmd, .async = &procs,
                             .max_procs = max_parallel_jobs)) {
                nob_minimal_log_level = original_log_level;
                nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
                return false;
            }

            const char* progress_label =
                (i + 1 == c_files_to_compile.count) ? "Complete!" : basename;
            update_progress(progress_label, i + 1, c_files_to_compile.count);
        }
    }

    nob_minimal_log_level = original_log_level;
    if (c_files_to_compile.count > 0) {
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
    }

    // Clean up temporary arrays
    size_t c_files_compiled = c_files_to_compile.count;
    nob_da_free(c_files_to_compile);
    for (size_t i = 0; i < c_corresponding_obj_files.count; i++) {
        free((char*)c_corresponding_obj_files.items[i]);
    }
    nob_da_free(c_corresponding_obj_files);

    size_t total_compiled = cpp_files_compiled + c_files_compiled;
    size_t total_files = cpp_sources.count + c_sources.count;
    if (total_compiled > 0) {
        nob_log(NOB_INFO, "Compiled %zu out of %zu files successfully!",
                total_compiled, total_files);
    } else {
        nob_log(NOB_INFO, "All %zu files are up to date!", total_files);
    }

    // Check if linking is needed
    const char* output_binary = "build/" PROJECT_NAME;
    bool needs_linking =
        (total_compiled > 0);  // If we compiled anything, we need to relink

    if (!needs_linking) {
        // Check if binary exists and is newer than all object files
        const char** obj_file_ptrs = (const char**)obj_files.items;
        int rebuild_result =
            nob_needs_rebuild(output_binary, obj_file_ptrs, obj_files.count);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if binary needs rebuild");
            return false;
        }
        needs_linking = (rebuild_result > 0);
    }

    if (!needs_linking) {
        nob_log(NOB_INFO, "Binary is up to date, skipping linking");
        nob_minimal_log_level = original_log_level;
        // Clean up and return
        nob_da_free(cpp_sources);
        nob_da_free(c_sources);
        for (size_t i = 0; i < obj_files.count; i++) {
            (void)obj_files.items[i];
        }
        nob_da_free(obj_files);
        nob_da_free(procs);
        return true;
    }

    nob_log(NOB_INFO, "Linking binary...");
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
