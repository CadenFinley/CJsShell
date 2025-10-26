#ifndef CJSH_NOB_COMPILE_H
#define CJSH_NOB_COMPILE_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nob_dependencies.h"
#include "nob_sources.h"
#include "nob_toolchain.h"

static inline bool capture_git_output(const char* const* args, size_t arg_count,
                                      Nob_String_Builder* output) {
    if (output == NULL) {
        return false;
    }

    const char* temp_path = "build/.git_info_tmp";
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "git");
    for (size_t i = 0; i < arg_count; ++i) {
        nob_cmd_append(&cmd, args[i]);
    }

    if (!nob_cmd_run(&cmd, .stdout_path = temp_path)) {
        return false;
    }

    bool ok = nob_read_entire_file(temp_path, output);
    remove(temp_path);
    if (!ok) {
        return false;
    }

    nob_sb_append_null(output);
    return true;
}

static inline bool compute_git_hash_string(char* buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    Nob_String_Builder hash_output = {0};
    const char* rev_args[] = {"rev-parse", "--short", "HEAD"};
    if (!capture_git_output(rev_args, NOB_ARRAY_LEN(rev_args), &hash_output)) {
        nob_da_free(hash_output);
        return false;
    }

    char* hash_data = hash_output.items;
    if (hash_data == NULL) {
        nob_da_free(hash_output);
        return false;
    }

    size_t hash_len = strlen(hash_data);
    while (hash_len > 0 && (hash_data[hash_len - 1] == '\n' || hash_data[hash_len - 1] == '\r' ||
                            hash_data[hash_len - 1] == ' ' || hash_data[hash_len - 1] == '\t')) {
        hash_data[--hash_len] = '\0';
    }

    if (hash_len == 0) {
        nob_da_free(hash_output);
        return false;
    }

    Nob_String_Builder status_output = {0};
    const char* status_args[] = {"status", "--porcelain"};
    bool dirty = false;
    if (capture_git_output(status_args, NOB_ARRAY_LEN(status_args), &status_output)) {
        size_t status_len = status_output.count > 0 ? status_output.count - 1 : 0;
        for (size_t i = 0; i < status_len; ++i) {
            char c = status_output.items[i];
            if (!isspace((unsigned char)c)) {
                dirty = true;
                break;
            }
        }
    }

    snprintf(buffer, buffer_size, "%s%s", hash_data, dirty ? "-dirty" : "");

    nob_da_free(hash_output);
    nob_da_free(status_output);
    return true;
}

static inline bool string_array_contains(const String_Array* array, const char* value) {
    for (size_t i = 0; i < array->count; i++) {
        if (strcmp(array->items[i], value) == 0) {
            return true;
        }
    }
    return false;
}

static inline bool parse_dependency_file(const char* dep_path, String_Array* deps) {
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
            while (j < content.count && (content.items[j] == '\n' || content.items[j] == '\r')) {
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

static inline int needs_rebuild_with_dependency_file(const char* obj_path, const char* source_path,
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
        nob_log(NOB_WARNING, "Failed to parse dependency file %s. Forcing rebuild.", dep_path);
        return 1;
    }

    int rebuild_result = nob_needs_rebuild(obj_path, (const char**)inputs.items, inputs.count);

    for (size_t i = 1; i < inputs.count; i++) {
        free((char*)inputs.items[i]);
    }
    nob_da_free(inputs);

    return rebuild_result;
}

static inline bool nob_cmd_run_with_spinner(Nob_Cmd* cmd, const char* label) {
    if (label == NULL) {
        label = "Working";
    }

    extern bool nob_suppress_cmd_output;
    nob_suppress_cmd_output = true;

    nob_log(NOB_INFO, "%s...", label);
    Nob_Proc proc = nob__cmd_start_process(*cmd, NULL, NULL, NULL);

    nob_suppress_cmd_output = false;

    if (proc == NOB_INVALID_PROC) {
        cmd->count = 0;
        return false;
    }

    bool wait_result = nob_proc_wait(proc);
    if (!wait_result) {
        cmd->count = 0;
        return false;
    }

    nob_log(NOB_INFO, "%s complete.", label);
    cmd->count = 0;
    return true;
}

typedef struct {
    String_Array arguments;
    char* file;
    char* output;
} Compile_Command_Entry;

typedef struct {
    Compile_Command_Entry* items;
    size_t count;
    size_t capacity;
} Compile_Command_List;

static inline void free_compile_command_entry(Compile_Command_Entry* entry) {
    if (entry == NULL) {
        return;
    }

    for (size_t i = 0; i < entry->arguments.count; ++i) {
        if (entry->arguments.items[i] != NULL) {
            free((char*)entry->arguments.items[i]);
        }
    }
    nob_da_free(entry->arguments);
    free(entry->file);
    free(entry->output);
    entry->arguments.items = NULL;
    entry->arguments.count = 0;
    entry->arguments.capacity = 0;
    entry->file = NULL;
    entry->output = NULL;
}

static inline void free_compile_command_list(Compile_Command_List* list) {
    if (list == NULL) {
        return;
    }

    for (size_t i = 0; i < list->count; ++i) {
        free_compile_command_entry(&list->items[i]);
    }
    nob_da_free(*list);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static inline bool add_compile_command_entry(Compile_Command_List* list, Nob_Cmd* cmd,
                                             const char* file, const char* output) {
    if (list == NULL || cmd == NULL || file == NULL) {
        return false;
    }

    Compile_Command_Entry entry = {0};

    for (size_t i = 0; i < cmd->count; ++i) {
        const char* arg = cmd->items[i];
        if (arg == NULL) {
            continue;
        }

        char* copy = strdup(arg);
        if (copy == NULL) {
            nob_log(NOB_ERROR, "Failed to duplicate compile command argument for %s", file);
            free_compile_command_entry(&entry);
            return false;
        }
        nob_da_append(&entry.arguments, copy);
    }

    entry.file = strdup(file);
    if (entry.file == NULL) {
        nob_log(NOB_ERROR, "Failed to duplicate source path for compile_commands entry: %s", file);
        free_compile_command_entry(&entry);
        return false;
    }

    if (output != NULL) {
        entry.output = strdup(output);
        if (entry.output == NULL) {
            nob_log(NOB_ERROR, "Failed to duplicate object path for compile_commands entry: %s",
                    file);
            free_compile_command_entry(&entry);
            return false;
        }
    }

    nob_da_append(list, entry);
    return true;
}

static inline void append_json_string(Nob_String_Builder* sb, const char* value) {
    nob_da_append(sb, '"');
    if (value != NULL) {
        const unsigned char* ptr = (const unsigned char*)value;
        while (*ptr) {
            unsigned char c = *ptr++;
            switch (c) {
                case '\\':
                    nob_sb_append_cstr(sb, "\\\\");
                    break;
                case '"':
                    nob_sb_append_cstr(sb, "\\\"");
                    break;
                case '\n':
                    nob_sb_append_cstr(sb, "\\n");
                    break;
                case '\r':
                    nob_sb_append_cstr(sb, "\\r");
                    break;
                case '\t':
                    nob_sb_append_cstr(sb, "\\t");
                    break;
                default:
                    if (c < 0x20) {
                        char buffer[7];
                        snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                        nob_sb_append_cstr(sb, buffer);
                    } else {
                        nob_da_append(sb, (char)c);
                    }
                    break;
            }
        }
    }
    nob_da_append(sb, '"');
}

static inline bool write_compile_commands_file(const char* directory,
                                               const Compile_Command_List* list) {
    if (directory == NULL || list == NULL) {
        return false;
    }

    Nob_String_Builder json = {0};
    nob_sb_append_cstr(&json, "[\n");

    for (size_t i = 0; i < list->count; ++i) {
        const Compile_Command_Entry* entry = &list->items[i];
        nob_sb_append_cstr(&json, "  {\n    \"directory\": ");
        append_json_string(&json, directory);
        nob_sb_append_cstr(&json, ",\n    \"file\": ");
        append_json_string(&json, entry->file != NULL ? entry->file : "");
        if (entry->output != NULL) {
            nob_sb_append_cstr(&json, ",\n    \"output\": ");
            append_json_string(&json, entry->output);
        }
        nob_sb_append_cstr(&json, ",\n    \"arguments\": [");

        if (entry->arguments.count > 0) {
            nob_sb_append_cstr(&json, "\n");
            for (size_t j = 0; j < entry->arguments.count; ++j) {
                nob_sb_append_cstr(&json, "      ");
                append_json_string(&json, entry->arguments.items[j]);
                if (j + 1 < entry->arguments.count) {
                    nob_sb_append_cstr(&json, ",");
                }
                nob_sb_append_cstr(&json, "\n");
            }
            nob_sb_append_cstr(&json, "    ]\n");
        } else {
            nob_sb_append_cstr(&json, "]\n");
        }

        nob_sb_append_cstr(&json, "  }");
        if (i + 1 < list->count) {
            nob_sb_append_cstr(&json, ",");
        }
        nob_sb_append_cstr(&json, "\n");
    }

    nob_sb_append_cstr(&json, "]\n");

    bool ok = nob_write_entire_file("compile_commands.json", json.items, json.count);
    nob_sb_free(json);
    return ok;
}

static inline bool append_nob_compile_command(Compile_Command_List* list) {
    if (list == NULL) {
        return false;
    }

    const char* nob_source = "toolchain/nob/nob.c";
#ifdef _WIN32
    const char* nob_output = "toolchain/nob/nob.exe";
#else
    const char* nob_output = "toolchain/nob/nob";
#endif

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, NOB_REBUILD_URSELF(nob_output, nob_source));

    bool ok = add_compile_command_entry(list, &cmd, nob_source, nob_output);
    nob_cmd_free(cmd);
    return ok;
}

static inline bool generate_assembly_for_source(const char* source, bool is_cpp) {
    const char* basename = strrchr(source, '/');
    if (basename)
        basename++;
    else
        basename = source;

    Nob_String_Builder asm_name = {0};
    nob_sb_append_cstr(&asm_name, "build/asm/");
    size_t base_len = strlen(basename);

    if (is_cpp && base_len > 4 && strcmp(basename + base_len - 4, ".cpp") == 0) {
        nob_sb_append_buf(&asm_name, basename, base_len - 4);
        nob_sb_append_cstr(&asm_name, ".s");
    } else if (!is_cpp && base_len > 2 && strcmp(basename + base_len - 2, ".c") == 0) {
        nob_sb_append_buf(&asm_name, basename, base_len - 2);
        nob_sb_append_cstr(&asm_name, ".s");
    } else {
        nob_sb_append_cstr(&asm_name, basename);
        nob_sb_append_cstr(&asm_name, ".s");
    }
    nob_sb_append_null(&asm_name);

    Nob_Cmd cmd = {0};
    bool success = false;

    if (is_cpp) {
        if (!setup_build_flags(&cmd)) {
            nob_sb_free(asm_name);
            return false;
        }
    } else {
        if (!setup_c_build_flags(&cmd)) {
            nob_sb_free(asm_name);
            return false;
        }
    }

    if (g_generate_readable_asm) {
        nob_cmd_append(&cmd, "-fverbose-asm");
        nob_cmd_append(&cmd, "-fno-asynchronous-unwind-tables");
        nob_cmd_append(&cmd, "-fno-dwarf2-cfi-asm");
    }

    nob_cmd_append(&cmd, "-S", source, "-o", asm_name.items);

    if (nob_cmd_run(&cmd)) {
        success = true;
    } else {
        nob_log(NOB_ERROR, "Failed to generate assembly for %s", source);
    }

    nob_cmd_free(cmd);
    nob_sb_free(asm_name);
    return success;
}

static inline bool compile_cjsh(int override_parallel_jobs, bool generate_compile_commands) {
    nob_log(NOB_INFO, "Compiling " PROJECT_NAME "...");
    Compile_Command_List compile_command_list = {0};
    char* compile_commands_directory = NULL;
    bool capture_compile_commands = generate_compile_commands;

    if (capture_compile_commands) {
        const char* cwd_temp = nob_get_current_dir_temp();
        if (cwd_temp != NULL) {
            compile_commands_directory = strdup(cwd_temp);
            if (compile_commands_directory == NULL) {
                nob_log(NOB_ERROR, "Failed to allocate directory string for compile_commands.json");
                capture_compile_commands = false;
            }
        } else {
            nob_log(NOB_ERROR, "Failed to determine current directory for compile_commands.json");
            capture_compile_commands = false;
        }
    }

    char git_hash_buffer[64] = {0};
    const char* env_git_hash = getenv("CJSH_GIT_HASH_OVERRIDE");
    if (env_git_hash != NULL && env_git_hash[0] != '\0') {
        snprintf(git_hash_buffer, sizeof(git_hash_buffer), "%s", env_git_hash);
        nob_set_git_hash_define(git_hash_buffer);
        nob_log(NOB_INFO, "Embedding git revision from CJSH_GIT_HASH_OVERRIDE: %s",
                git_hash_buffer);
    } else if (compute_git_hash_string(git_hash_buffer, sizeof(git_hash_buffer))) {
        nob_set_git_hash_define(git_hash_buffer);
        nob_log(NOB_INFO, "Embedding git revision: %s", git_hash_buffer);
    } else {
        nob_set_git_hash_define("unknown");
        nob_log(NOB_WARNING, "Unable to determine git revision; embedding 'unknown'");
    }

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

        int rebuild_result =
            needs_rebuild_with_dependency_file(obj_name.items, source, dep_name.items);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if %s needs rebuild", source);
            nob_sb_free(dep_name);
            nob_sb_free(obj_name);
            return false;
        }

        if (rebuild_result > 0) {
            nob_da_append(&files_to_compile, source);
            nob_da_append(&corresponding_obj_files, strdup(obj_name.items));
            nob_da_append(&corresponding_dep_files, strdup(dep_name.items));
        }

        if (capture_compile_commands) {
            Nob_Cmd cc_cmd = {0};
            if (!setup_build_flags(&cc_cmd)) {
                nob_log(NOB_ERROR, "Failed to prepare compile command for %s", source);
                capture_compile_commands = false;
            } else {
                nob_cmd_append(&cc_cmd, "-MMD", "-MF", dep_name.items, "-MT", obj_name.items);
                nob_cmd_append(&cc_cmd, "-c", source, "-o", obj_name.items);
                if (!add_compile_command_entry(&compile_command_list, &cc_cmd, source,
                                               obj_name.items)) {
                    capture_compile_commands = false;
                }
            }
            nob_cmd_free(cc_cmd);
        }

        if (capture_compile_commands) {
            Nob_Cmd cc_cmd = {0};
            if (!setup_c_build_flags(&cc_cmd)) {
                nob_log(NOB_ERROR, "Failed to prepare C compile command for %s", source);
                capture_compile_commands = false;
            } else {
                nob_cmd_append(&cc_cmd, "-MMD", "-MF", dep_name.items, "-MT", obj_name.items);
                nob_cmd_append(&cc_cmd, "-c", source, "-o", obj_name.items);
                if (!add_compile_command_entry(&compile_command_list, &cc_cmd, source,
                                               obj_name.items)) {
                    capture_compile_commands = false;
                }
            }
            nob_cmd_free(cc_cmd);
        }

        nob_da_append(&obj_files, strdup(obj_name.items));
        nob_sb_free(dep_name);
        nob_sb_free(obj_name);
    }

    int max_parallel_jobs;
    if (override_parallel_jobs > 0) {
        max_parallel_jobs = override_parallel_jobs;
    } else {
        size_t files_needing_compilation = files_to_compile.count;

        if (files_needing_compilation == 0) {
            max_parallel_jobs = 1;
        } else if (files_needing_compilation <= 2) {
            max_parallel_jobs = 1;
        } else if (files_needing_compilation <= 8 || total_source_files <= 8) {
            max_parallel_jobs = (int)files_needing_compilation < max_cpu_cores / 2
                                    ? (int)files_needing_compilation
                                    : max_cpu_cores / 2;
            if (max_parallel_jobs < 1)
                max_parallel_jobs = 1;
        } else {
            max_parallel_jobs = max_cpu_cores;
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
            nob_log(NOB_INFO, "Using %d parallel compilation jobs (auto) for %zu files",
                    max_parallel_jobs, files_to_compile.count);
        }
        nob_log(NOB_INFO,
                "Starting parallel compilation of %zu C++ files (skipping %zu "
                "up-to-date)...",
                files_to_compile.count, cpp_sources.count - files_to_compile.count);

        extern size_t nob_compile_current;
        extern size_t nob_compile_total;
        extern const char* nob_compile_filename;
        nob_compile_total = files_to_compile.count;

        for (size_t i = 0; i < files_to_compile.count; i++) {
            Nob_Cmd cmd = {0};
            if (!setup_build_flags(&cmd)) {
                return false;
            }

            nob_cmd_append(&cmd, "-MMD", "-MF", corresponding_dep_files.items[i], "-MT",
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

            nob_compile_current = i + 1;
            nob_compile_filename = basename;

            if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_parallel_jobs)) {
                nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
                nob_compile_total = 0;
                nob_compile_filename = NULL;
                return false;
            }
        }
    }

    if (files_to_compile.count > 0) {
        nob_log(NOB_INFO, "Waiting for C++ compilation to complete...");
        if (!nob_procs_flush(&procs)) {
            nob_log(NOB_ERROR, "C++ compilation failed");

            extern size_t nob_compile_total;
            extern const char* nob_compile_filename;
            nob_compile_total = 0;
            nob_compile_filename = NULL;
            return false;
        }
        nob_log(NOB_INFO, "All %zu C++ files compiled successfully", files_to_compile.count);

        extern size_t nob_compile_total;
        extern const char* nob_compile_filename;
        nob_compile_total = 0;
        nob_compile_filename = NULL;
    }

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

        int rebuild_result =
            needs_rebuild_with_dependency_file(obj_name.items, source, dep_name.items);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if %s needs rebuild", source);
            nob_sb_free(dep_name);
            nob_sb_free(obj_name);
            return false;
        }

        if (rebuild_result > 0) {
            nob_da_append(&c_files_to_compile, source);
            nob_da_append(&c_corresponding_obj_files, strdup(obj_name.items));
            nob_da_append(&c_corresponding_dep_files, strdup(dep_name.items));
        }

        nob_da_append(&obj_files, strdup(obj_name.items));
        nob_sb_free(dep_name);
        nob_sb_free(obj_name);
    }

    if (c_files_to_compile.count > 0) {
        if (override_parallel_jobs <= 0) {
            size_t c_files_needing_compilation = c_files_to_compile.count;

            if (c_files_needing_compilation <= 2) {
                max_parallel_jobs = 1;
            } else if (c_files_needing_compilation <= 8 || total_source_files <= 8) {
                max_parallel_jobs = (int)c_files_needing_compilation < max_cpu_cores / 2
                                        ? (int)c_files_needing_compilation
                                        : max_cpu_cores / 2;
                if (max_parallel_jobs < 1)
                    max_parallel_jobs = 1;
            } else {
                max_parallel_jobs = max_cpu_cores;
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
            nob_log(NOB_INFO, "Using %d parallel compilation jobs (auto) for %zu C files",
                    max_parallel_jobs, c_files_to_compile.count);
        }

        extern size_t nob_compile_current;
        extern size_t nob_compile_total;
        extern const char* nob_compile_filename;
        nob_compile_total = c_files_to_compile.count;

        for (size_t i = 0; i < c_files_to_compile.count; i++) {
            Nob_Cmd cmd = {0};
            if (!setup_c_build_flags(&cmd)) {
                return false;
            }

            nob_cmd_append(&cmd, "-MMD", "-MF", c_corresponding_dep_files.items[i], "-MT",
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

            nob_compile_current = i + 1;
            nob_compile_filename = basename;

            if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_parallel_jobs)) {
                nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
                nob_compile_total = 0;
                nob_compile_filename = NULL;
                return false;
            }
        }
    }

    if (c_files_to_compile.count > 0) {
        nob_log(NOB_INFO, "Waiting for C compilation to complete...");
        if (!nob_procs_flush(&procs)) {
            nob_log(NOB_ERROR, "C compilation failed");

            extern size_t nob_compile_total;
            extern const char* nob_compile_filename;
            nob_compile_total = 0;
            nob_compile_filename = NULL;
            return false;
        }

        extern size_t nob_compile_total;
        extern const char* nob_compile_filename;
        nob_compile_total = 0;
        nob_compile_filename = NULL;
    }

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
        nob_log(NOB_INFO, "Compiled %zu out of %zu files successfully!", total_compiled,
                total_files);
    } else {
        nob_log(NOB_INFO, "All %zu files are up to date!", total_files);
    }

    if (capture_compile_commands) {
        if (!append_nob_compile_command(&compile_command_list)) {
            nob_log(NOB_ERROR, "Failed to add compile command entry for toolchain/nob/nob.c");
            capture_compile_commands = false;
        }
    }

    if (capture_compile_commands && compile_commands_directory != NULL) {
        if (write_compile_commands_file(compile_commands_directory, &compile_command_list)) {
            nob_log(NOB_INFO, "Generated compile_commands.json with %zu entries",
                    compile_command_list.count);
        } else {
            nob_log(NOB_ERROR, "Failed to write compile_commands.json");
        }
    } else if (generate_compile_commands && !capture_compile_commands) {
        nob_log(NOB_WARNING, "Skipping compile_commands.json generation due to previous errors");
    }

    free_compile_command_list(&compile_command_list);
    if (compile_commands_directory != NULL) {
        free(compile_commands_directory);
        compile_commands_directory = NULL;
    }

    const char* output_binary = "build/" PROJECT_NAME;
    bool needs_linking = (total_compiled > 0);

    if (!needs_linking) {
        const char** obj_file_ptrs = (const char**)obj_files.items;
        int rebuild_result = nob_needs_rebuild(output_binary, obj_file_ptrs, obj_files.count);
        if (rebuild_result < 0) {
            nob_log(NOB_ERROR, "Failed to check if binary needs rebuild");
            return false;
        }
        needs_linking = (rebuild_result > 0);
    }

    if (!needs_linking) {
        nob_log(NOB_INFO, "Binary is up to date, skipping linking");

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
    Nob_Cmd link_cmd = {0};

    const char* linker = get_linker();
    nob_cmd_append(&link_cmd, linker);

    if (g_debug_build) {
        nob_cmd_append(&link_cmd, "-g");
    } else {
        nob_cmd_append(&link_cmd, get_lto_flag_for_compiler(linker));
    }

#ifdef PLATFORM_MACOS
    if (strcmp(linker, "clang++") == 0) {
        nob_cmd_append(&link_cmd, "-stdlib=libc++");
    }
#ifdef ARCH_ARM64
    nob_cmd_append(&link_cmd, "-arch", "arm64");
#elif defined(ARCH_X86_64)
    nob_cmd_append(&link_cmd, "-arch", "x86_64");
#endif
    if (!g_debug_build) {
        nob_cmd_append(&link_cmd, "-Wl,-dead_strip", "-Wl,-dead_strip_dylibs");
        if (g_minimal_build) {
            nob_cmd_append(&link_cmd, "-Wl,-no_compact_unwind");
            nob_cmd_append(&link_cmd, "-Wl,-no_function_starts");
        }
    }
#endif

#ifdef PLATFORM_LINUX
    if (!g_debug_build && strcmp(linker, "g++") == 0) {
        nob_cmd_append(&link_cmd, "-static-libgcc", "-static-libstdc++");
    }
    if (!g_debug_build) {
        nob_cmd_append(&link_cmd, "-Wl,--gc-sections", "-Wl,--as-needed");
        if (g_minimal_build) {
            nob_cmd_append(&link_cmd, "-Wl,--strip-all", "-Wl,--discard-all");
            nob_cmd_append(&link_cmd, "-Wl,--no-undefined", "-Wl,--compress-debug-sections=zlib");
            nob_cmd_append(&link_cmd, "-Wl,-O2");
            nob_cmd_append(&link_cmd, "-Wl,--hash-style=gnu");
        }
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

    if (g_debug_build) {
        nob_cmd_append(&link_cmd, "-fsanitize=address");
    }

#if defined(PLATFORM_LINUX) || defined(PLATFORM_UNIX)
    nob_cmd_append(&link_cmd, "-ldl");
#endif

    for (size_t i = 0; i < build_config.external_library_paths_count; i++) {
        nob_cmd_append(&link_cmd, build_config.external_library_paths[i]);
    }

    if (!nob_cmd_run_with_spinner(&link_cmd, "Linking cjsh")) {
        nob_log(NOB_ERROR, "Linking failed");
        return false;
    }

    const char* strip_env = getenv("CJSH_STRIP_BINARY");
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    bool strip_requested = true;
    if (strip_env != NULL && strip_env[0] != '\0') {
        strip_requested = strcmp(strip_env, "0") != 0;
    }
    if (strip_requested && !g_debug_build) {
        Nob_Cmd strip_cmd = {0};
#ifdef PLATFORM_MACOS
        nob_cmd_append(&strip_cmd, "strip", "-x", output_binary);
#elif defined(PLATFORM_LINUX)
        nob_cmd_append(&strip_cmd, "strip", "--strip-unneeded", output_binary);
#endif
        if (strip_cmd.count > 0) {
            nob_log(NOB_INFO, "Stripping symbols for smaller binary size...");
            if (!nob_cmd_run(&strip_cmd)) {
                nob_log(NOB_WARNING,
                        "Failed to strip binary; continuing with unstripped "
                        "output");
            }
        }
    }
#endif
#if !(defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX))
    (void)strip_env;
#endif

    nob_da_free(cpp_sources);
    nob_da_free(c_sources);
    for (size_t i = 0; i < obj_files.count; i++) {
        (void)obj_files.items[i];
    }
    nob_da_free(obj_files);
    nob_da_free(procs);

    if (g_generate_asm) {
        nob_log(NOB_INFO, "Generating assembly files...");

        String_Array all_cpp_sources = {0};
        String_Array all_c_sources = {0};

        if (!collect_sources(&all_cpp_sources)) {
            nob_log(NOB_ERROR, "Failed to collect sources for assembly generation");
            return false;
        }

        if (!collect_c_sources(&all_c_sources)) {
            nob_log(NOB_ERROR, "Failed to collect C sources for assembly generation");
            nob_da_free(all_cpp_sources);
            return false;
        }

        size_t total_asm_files = all_cpp_sources.count + all_c_sources.count;
        size_t asm_generated = 0;

        for (size_t i = 0; i < all_cpp_sources.count; i++) {
            if (generate_assembly_for_source(all_cpp_sources.items[i], true)) {
                asm_generated++;
            }
        }

        for (size_t i = 0; i < all_c_sources.count; i++) {
            if (generate_assembly_for_source(all_c_sources.items[i], false)) {
                asm_generated++;
            }
        }

        nob_da_free(all_cpp_sources);
        nob_da_free(all_c_sources);

        if (asm_generated == total_asm_files) {
            nob_log(NOB_INFO, "Generated %zu assembly files in build/asm", asm_generated);
        } else {
            nob_log(NOB_WARNING, "Generated %zu out of %zu assembly files", asm_generated,
                    total_asm_files);
        }
    }

    return true;
}

#endif
