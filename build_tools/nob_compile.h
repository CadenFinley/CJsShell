#ifndef CJSH_NOB_COMPILE_H
#define CJSH_NOB_COMPILE_H

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "nob_dependencies.h"
#include "nob_progress.h"
#include "nob_sources.h"
#include "nob_toolchain.h"

static inline bool string_array_contains(const String_Array* array,
                                         const char* value) {
    for (size_t i = 0; i < array->count; i++) {
        if (strcmp(array->items[i], value) == 0) {
            return true;
        }
    }
    return false;
}

static inline bool parse_dependency_file(const char* dep_path,
                                         String_Array* deps) {
    Nob_String_Builder content = {0};
    if (!nob_read_entire_file(dep_path, &content)) {
        nob_da_free(content);
        return false;
    }

    Nob_String_Builder sanitized = {0};
    for (size_t i = 0; i < content.count; i++) {
        char c = content.items[i];
        if (c == '\\') {
            size_t j = i + 1;
            bool had_newline = false;
            while (j < content.count &&
                   (content.items[j] == '\n' || content.items[j] == '\r')) {
                had_newline = true;
                j++;
            }
            if (had_newline) {
                i = j - 1;
                continue;
            }
        }

        if (c == '\n' || c == '\r') {
            c = ' ';
        }

        nob_da_append(&sanitized, c);
    }
    nob_sb_append_null(&sanitized);

    char* data = sanitized.items;
    char* colon = strchr(data, ':');
    if (colon == NULL) {
        nob_da_free(content);
        nob_da_free(sanitized);
        return false;
    }
    *colon = ' ';

    bool first_token = true;
    char* ptr = data;
    while (*ptr) {
        while (*ptr && isspace((unsigned char)*ptr)) {
            ptr++;
        }

        if (!*ptr) {
            break;
        }

        char* end = ptr;
        while (*end && !isspace((unsigned char)*end)) {
            end++;
        }

        char saved = *end;
        *end = '\0';

        if (first_token) {
            first_token = false;
        } else {
            size_t token_len = strlen(ptr);
            if (token_len > 0 && ptr[token_len - 1] == ':') {
                // Skip phony targets produced by dependency generators
            } else if (!string_array_contains(deps, ptr)) {
                nob_da_append(deps, strdup(ptr));
            }
        }

        *end = saved;
        ptr = end;
    }

    nob_da_free(content);
    nob_da_free(sanitized);
    return true;
}

static inline int needs_rebuild_with_dependency_file(const char* obj_path,
                                                     const char* source_path,
                                                     const char* dep_path) {
    int dep_exists = nob_file_exists(dep_path);
    if (dep_exists < 0) {
        return -1;
    }

    if (dep_exists == 0) {
        return 1;
    }

    String_Array inputs = {0};
    nob_da_append(&inputs, source_path);

    if (!parse_dependency_file(dep_path, &inputs)) {
        for (size_t i = 1; i < inputs.count; i++) {
            free((char*)inputs.items[i]);
        }
        nob_da_free(inputs);
        nob_log(NOB_WARNING,
                "Failed to parse dependency file %s. Forcing rebuild.",
                dep_path);
        return 1;
    }

    int rebuild_result =
        nob_needs_rebuild(obj_path, (const char**)inputs.items, inputs.count);

    for (size_t i = 1; i < inputs.count; i++) {
        free((char*)inputs.items[i]);
    }
    nob_da_free(inputs);

    return rebuild_result;
}

static inline bool compile_cjsh(int override_parallel_jobs) {
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
    int max_cpu_cores = nob_nprocs();
    if (max_cpu_cores <= 0)
        max_cpu_cores = 4;

    size_t total_source_files = cpp_sources.count + c_sources.count;

    Nob_Log_Level original_log_level = nob_minimal_log_level;
    size_t completed_cpp_files = 0;

    // First pass: determine which files need to be compiled
    String_Array files_to_compile = {0};
    String_Array corresponding_obj_files = {0};
    String_Array corresponding_dep_files = {0};

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

        // Determine dependency file path
        Nob_String_Builder dep_name = {0};
        nob_sb_append_cstr(&dep_name, "build/obj/");
        if (base_len > 4 && strcmp(basename + base_len - 4, ".cpp") == 0) {
            nob_sb_append_buf(&dep_name, basename, base_len - 4);
            nob_sb_append_cstr(&dep_name, ".d");
        } else {
            nob_sb_append_cstr(&dep_name, basename);
            nob_sb_append_cstr(&dep_name, ".d");
        }
        nob_sb_append_null(&dep_name);

        // Check if compilation is needed
        int rebuild_result = needs_rebuild_with_dependency_file(
            obj_name.items, source, dep_name.items);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if %s needs rebuild", source);
            nob_sb_free(dep_name);
            nob_sb_free(obj_name);
            return false;
        }

        if (rebuild_result > 0) {
            // File needs to be compiled
            nob_da_append(&files_to_compile, source);
            nob_da_append(&corresponding_obj_files, strdup(obj_name.items));
            nob_da_append(&corresponding_dep_files, strdup(dep_name.items));
        }

        // Always add to obj_files for linking
        nob_da_append(&obj_files, strdup(obj_name.items));
        nob_sb_free(dep_name);
        nob_sb_free(obj_name);
    }

    // Calculate parallel jobs based on files that actually need compilation
    // (but consider total project size for context)
    int max_parallel_jobs;
    if (override_parallel_jobs > 0) {
        max_parallel_jobs = override_parallel_jobs;  // Use user-specified value
    } else {
        // Automatic calculation based on files to compile and project size:
        size_t files_needing_compilation = files_to_compile.count;

        if (files_needing_compilation == 0) {
            max_parallel_jobs = 1;  // Doesn't matter, no compilation needed
        } else if (files_needing_compilation <= 2) {
            max_parallel_jobs = 1;  // Sequential for very few files
        } else if (files_needing_compilation <= 8 || total_source_files <= 8) {
            // For small compilations or small projects, use limited parallelism
            max_parallel_jobs =
                (int)files_needing_compilation < max_cpu_cores / 2
                    ? (int)files_needing_compilation
                    : max_cpu_cores / 2;
            if (max_parallel_jobs < 1)
                max_parallel_jobs = 1;
        } else {
            max_parallel_jobs =
                max_cpu_cores;  // Use all cores for larger compilations
        }
    }

    if (files_to_compile.count == 0) {
        nob_log(NOB_INFO, "All C++ files are up to date, skipping compilation");
    } else {
        if (override_parallel_jobs > 0) {
            nob_log(NOB_INFO,
                    "Using %d parallel compilation jobs (user override) for "
                    "%zu files",
                    max_parallel_jobs, files_to_compile.count);
        } else {
            nob_log(NOB_INFO,
                    "Using %d parallel compilation jobs (auto) for %zu files",
                    max_parallel_jobs, files_to_compile.count);
        }
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

            nob_cmd_append(&cmd, "-MMD", "-MF",
                           corresponding_dep_files.items[i], "-MT",
                           corresponding_obj_files.items[i]);
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
    for (size_t i = 0; i < corresponding_dep_files.count; i++) {
        free((char*)corresponding_dep_files.items[i]);
    }
    nob_da_free(corresponding_dep_files);

    // Second pass: determine which C files need to be compiled
    String_Array c_files_to_compile = {0};
    String_Array c_corresponding_obj_files = {0};
    String_Array c_corresponding_dep_files = {0};

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

        Nob_String_Builder dep_name = {0};
        nob_sb_append_cstr(&dep_name, "build/obj/");
        if (base_len > 2 && strcmp(basename + base_len - 2, ".c") == 0) {
            nob_sb_append_buf(&dep_name, basename, base_len - 2);
            nob_sb_append_cstr(&dep_name, ".c.d");
        } else {
            nob_sb_append_cstr(&dep_name, basename);
            nob_sb_append_cstr(&dep_name, ".d");
        }
        nob_sb_append_null(&dep_name);

        // Check if compilation is needed
        int rebuild_result = needs_rebuild_with_dependency_file(
            obj_name.items, source, dep_name.items);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if %s needs rebuild", source);
            nob_sb_free(dep_name);
            nob_sb_free(obj_name);
            return false;
        }

        if (rebuild_result > 0) {
            // File needs to be compiled
            nob_da_append(&c_files_to_compile, source);
            nob_da_append(&c_corresponding_obj_files, strdup(obj_name.items));
            nob_da_append(&c_corresponding_dep_files, strdup(dep_name.items));
        }

        // Always add to obj_files for linking
        nob_da_append(&obj_files, strdup(obj_name.items));
        nob_sb_free(dep_name);
        nob_sb_free(obj_name);
    }

    // Recalculate parallel jobs for C files if needed
    if (c_files_to_compile.count > 0) {
        if (override_parallel_jobs <= 0) {
            // Only recalculate if we're using automatic calculation
            size_t c_files_needing_compilation = c_files_to_compile.count;

            if (c_files_needing_compilation <= 2) {
                max_parallel_jobs = 1;  // Sequential for very few files
            } else if (c_files_needing_compilation <= 8 ||
                       total_source_files <= 8) {
                // For small compilations or small projects, use limited
                // parallelism
                max_parallel_jobs =
                    (int)c_files_needing_compilation < max_cpu_cores / 2
                        ? (int)c_files_needing_compilation
                        : max_cpu_cores / 2;
                if (max_parallel_jobs < 1)
                    max_parallel_jobs = 1;
            } else {
                max_parallel_jobs =
                    max_cpu_cores;  // Use all cores for larger compilations
            }
        }
    }

    if (c_files_to_compile.count == 0) {
        nob_log(NOB_INFO, "All C files are up to date, skipping compilation");
    } else {
        if (override_parallel_jobs > 0) {
            nob_log(NOB_INFO,
                    "Using %d parallel compilation jobs (user override) for "
                    "%zu C files",
                    max_parallel_jobs, c_files_to_compile.count);
        } else {
            nob_log(NOB_INFO,
                    "Using %d parallel compilation jobs (auto) for %zu C files",
                    max_parallel_jobs, c_files_to_compile.count);
        }
        nob_minimal_log_level = NOB_WARNING;
        for (size_t i = 0; i < c_files_to_compile.count; i++) {
            Nob_Cmd cmd = {0};
            if (!setup_c_build_flags(&cmd)) {
                nob_minimal_log_level = original_log_level;
                return false;
            }

            nob_cmd_append(&cmd, "-MMD", "-MF",
                           c_corresponding_dep_files.items[i], "-MT",
                           c_corresponding_obj_files.items[i]);
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
    for (size_t i = 0; i < c_corresponding_dep_files.count; i++) {
        free((char*)c_corresponding_dep_files.items[i]);
    }
    nob_da_free(c_corresponding_dep_files);

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
