#define NOB_IMPLEMENTATION
#include "nob.h"

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

// ANSI color codes for progress bar
#define NOB_ANSI_COLOR_GREEN   "\033[32m"
#define NOB_ANSI_COLOR_YELLOW  "\033[33m"
#define NOB_ANSI_COLOR_RED     "\033[31m"
#define NOB_ANSI_COLOR_RESET   "\033[0m"

#define PROJECT_NAME "cjsh"
#define VERSION "3.5.1"

// Architecture detection
#if defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_ARM64
#elif defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_X64)
    #define ARCH_X86_64
#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
    #define ARCH_X86
#else
    #define ARCH_UNKNOWN
#endif

// Platform detection
#if defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_MACOS
#elif defined(__linux__)
    #define PLATFORM_LINUX
#elif defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
#else
    #define PLATFORM_UNIX
#endif

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} String_Array;

// Build configuration - centralized file and directory declarations
typedef struct {
    const char **main_sources;
    size_t main_sources_count;
    
    const char **module_directories;
    size_t module_directories_count;
    
    const char **isocline_c_sources;
    size_t isocline_c_sources_count;
    
    const char **include_directories;
    size_t include_directories_count;
    
    const char **c_include_directories;
    size_t c_include_directories_count;
    
    const char **required_directories;
    size_t required_directories_count;
    
    const char **external_dependencies;
    size_t external_dependencies_count;
    
    const char **external_library_paths;
    size_t external_library_paths_count;
    
    const char **dependency_urls;
    size_t dependency_urls_count;
} Build_Config;

// Global build configuration
extern const Build_Config build_config;

// Forward declarations
bool compile_cjsh(void);
bool download_dependencies(void);
bool collect_sources(String_Array *sources);
bool collect_c_sources(String_Array *c_sources);
bool setup_build_flags(Nob_Cmd *cmd);
bool setup_c_build_flags(Nob_Cmd *cmd);
bool check_dependencies(void);
void print_help(void);
void print_version(void);
void print_dependencies(void);
bool create_required_directories(void);

// Progress bar functions
void draw_progress_bar(const char* phase, size_t current, size_t total, size_t width);
void clear_progress_line(void);
void update_progress(const char* phase, size_t current, size_t total);

// Build configuration implementation
const Build_Config build_config = {
    // Main C++ source files
    .main_sources = (const char*[]){
        "src/cjsh.cpp",
        "src/error_out.cpp", 
        "src/exec.cpp",
        "src/job_control.cpp",
        "src/main_loop.cpp",
        "src/shell.cpp",
        "src/signal_handler.cpp",
        "src/utils/libintl_shim.cpp"  // Add shim file first
    },
    .main_sources_count = 8,
    
    // Module directories to scan for C++ files
    .module_directories = (const char*[]){
        "src/builtin",
        "src/ai",
        "src/prompt",
        "src/prompt/modules",
        "src/script_interpreter",
        "src/plugins",
        "src/utils"  // Will skip libintl_shim.cpp since it's already included
    },
    .module_directories_count = 7,
    
    // Specific isocline C source files
    .isocline_c_sources = (const char*[]){
        "src/isocline/attr.c",
        "src/isocline/bbcode.c",
        "src/isocline/bbcode_colors.c",
        "src/isocline/common.c",
        "src/isocline/completers.c",
        "src/isocline/completions.c",
        "src/isocline/editline.c",
        "src/isocline/highlight.c",
        "src/isocline/history.c",
        "src/isocline/isocline.c",
        "src/isocline/stringbuf.c",
        "src/isocline/term.c",
        "src/isocline/tty.c",
        "src/isocline/tty_esc.c",
        "src/isocline/undo.c"
    },
    .isocline_c_sources_count = 15,
    
    // Include directories
    .include_directories = (const char*[]){
        "include",
        "include/isocline",
        "include/builtin",
        "include/utils",
        "include/prompt",
        "include/prompt/modules",
        "include/ai",
        "include/plugins",
        "include/script_interpreter",
        "build/vendor",  // For nlohmann/json
        "build/vendor/utf8proc"  // For utf8proc
    },
    .include_directories_count = 11,
    
    // C-specific include directories (for C files only)
    .c_include_directories = (const char*[]){
        "include",
        "include/isocline",
        "build/vendor/utf8proc"
    },
    .c_include_directories_count = 3,
    
    // Required build directories
    .required_directories = (const char*[]){
        "build",
        "build/obj",
        "build/vendor",
        "build/vendor/nlohmann",
        "build/vendor/utf8proc"
    },
    .required_directories_count = 5,
    
    // External dependencies (downloaded/built from source, no system packages)
    .external_dependencies = (const char*[]){
        "build/vendor/nlohmann/json.hpp",        // Downloaded from GitHub releases
        "build/vendor/utf8proc/libutf8proc.a"    // Built from source
    },
    .external_dependencies_count = 2,
    
    // External library paths for linking
    .external_library_paths = (const char*[]){
        "build/vendor/utf8proc/libutf8proc.a"
    },
    .external_library_paths_count = 1,
    
    // Dependency download URLs and paths
    .dependency_urls = (const char*[]){
        "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp",
        "https://github.com/JuliaStrings/utf8proc.git"
    },
    .dependency_urls_count = 2
};

// Progress bar implementation
void draw_progress_bar(const char* phase, size_t current, size_t total, size_t width) {
    if (total == 0) return;
    
    float progress = (float)current / (float)total;
    size_t filled = (size_t)(progress * width);
    
    printf("\r%s [%s", phase, NOB_ANSI_COLOR_GREEN);
    
    // Draw filled portion
    for (size_t i = 0; i < filled; i++) {
        printf("█");
    }
    
    printf("%s", NOB_ANSI_COLOR_RESET);
    
    // Draw empty portion
    for (size_t i = filled; i < width; i++) {
        printf("░");
    }
    
    printf("] %zu/%zu (%.1f%%) ", current, total, progress * 100.0f);
    fflush(stdout);
}

void clear_progress_line(void) {
    printf("\r\033[K"); // Clear current line
    fflush(stdout);
}

void update_progress(const char* phase, size_t current, size_t total) {
    draw_progress_bar(phase, current, total, 40);
    if (current == total) {
        printf("\n"); // New line when complete
        fflush(stdout);
    }
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    // Change to parent directory (project root)
    if (!nob_set_current_dir("..")) {
        nob_log(NOB_ERROR, "Could not change to parent directory");
        return 1;
    }
    
    // Parse command line arguments
    bool help = false;
    bool version = false;
    bool clean = false;
    bool debug = false;
    bool force_32bit = false;
    bool dependencies = false;
    
    // Skip the program name
    nob_shift_args(&argc, &argv);
    
    while (argc > 0) {
        char *arg = nob_shift_args(&argc, &argv);
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            help = true;
        } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
            version = true;
        } else if (strcmp(arg, "--clean") == 0) {
            clean = true;
        } else if (strcmp(arg, "--debug") == 0) {
            debug = true;
        } else if (strcmp(arg, "--force-32bit") == 0) {
            force_32bit = true;
        } else if (strcmp(arg, "--dependencies") == 0) {
            dependencies = true;
        } else {
            nob_log(NOB_ERROR, "Unknown argument: %s", arg);
            print_help();
            return 1;
        }
    }
    
    if (help) {
        print_help();
        return 0;
    }
    
    if (version) {
        print_version();
        return 0;
    }
    
    if (dependencies) {
        print_dependencies();
        return 0;
    }
    
    if (clean) {
        nob_log(NOB_INFO, "Cleaning build directory...");
        // Remove build directory if it exists
        if (nob_get_file_type("build") == NOB_FILE_DIRECTORY) {
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, "rm", "-rf", "build");
            if (!nob_cmd_run(&cmd)) {
                nob_log(NOB_ERROR, "Failed to clean build directory");
                return 1;
            }
        }
        nob_log(NOB_INFO, "Clean complete");
        return 0;
    }
    
    // Check dependencies
    if (!check_dependencies()) {
        nob_log(NOB_ERROR, "Dependency check failed");
        return 1;
    }
    
    // Create required directories first
    if (!create_required_directories()) {
        nob_log(NOB_ERROR, "Failed to create required directories");
        return 1;
    }
    
    // Download external dependencies if needed
    if (!download_dependencies()) {
        nob_log(NOB_ERROR, "Failed to download dependencies");
        return 1;
    }
    
    // Compile the project
    if (!compile_cjsh()) {
        nob_log(NOB_ERROR, "Compilation failed");
        return 1;
    }
    
    nob_log(NOB_INFO, "Build completed successfully!");
    nob_log(NOB_INFO, "Output binary: build/" PROJECT_NAME);
    
    return 0;
}

bool check_dependencies(void) {
    nob_log(NOB_INFO, "Checking dependencies...");
    
    // Check for C++ compiler (suppress output)
    Nob_Cmd cmd = {0};
    
    // Try g++ first (redirect output to suppress command display)
    nob_cmd_append(&cmd, "which", "g++");
    if (nob_cmd_run(&cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        return true;
    }
    
    // Try clang++
    cmd.count = 0;
    nob_cmd_append(&cmd, "which", "clang++");
    if (nob_cmd_run(&cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        return true;
    }
    
    nob_log(NOB_ERROR, "No C++ compiler found. Please install g++ or clang++");
    return false;
}

bool download_dependencies(void) {
    nob_log(NOB_INFO, "Checking external dependencies...");
    
    // Note: This build system avoids pkg-config and system packages
    // All dependencies are downloaded and built from source for consistency
    // Directory creation is handled by create_required_directories()
    // To add new dependencies, update the external_dependencies array in build_config
    
    // Download nlohmann/json if not present (using configuration URLs)
    const char *json_header_path = build_config.external_dependencies[0];  // "build/vendor/nlohmann/json.hpp"
    if (nob_get_file_type(json_header_path) != NOB_FILE_REGULAR) {
        nob_log(NOB_INFO, "Downloading nlohmann/json...");
        
        const char *json_url = build_config.dependency_urls[0];  // nlohmann/json URL
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "curl", "-L", "-o", json_header_path, json_url);
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_WARNING, "Failed to download with curl, trying wget...");
            cmd.count = 0;
            nob_cmd_append(&cmd, "wget", "-O", json_header_path, json_url);
            if (!nob_cmd_run(&cmd)) {
                nob_log(NOB_ERROR, "Failed to download nlohmann/json. Please download manually or install system package.");
                return false;
            }
        }
        nob_log(NOB_INFO, "Downloaded nlohmann/json successfully");
    }
    
    // Download and build utf8proc from source (using configuration URLs)
    // Check if utf8proc directory exists and contains files (not just empty)
    bool need_download = true;
    if (nob_get_file_type("build/vendor/utf8proc") == NOB_FILE_DIRECTORY) {
        // Check if directory contains files (specifically look for Makefile)
        if (nob_get_file_type("build/vendor/utf8proc/Makefile") == NOB_FILE_REGULAR) {
            need_download = false;
        } else {
            // Directory exists but is empty, remove it first
            nob_log(NOB_INFO, "Removing empty utf8proc directory...");
            Nob_Cmd cleanup_cmd = {0};
            nob_cmd_append(&cleanup_cmd, "rm", "-rf", "build/vendor/utf8proc");
            nob_cmd_run(&cleanup_cmd); // Don't fail if this fails
        }
    }
    
    if (need_download) {
        nob_log(NOB_INFO, "Downloading utf8proc...");
        
        const char *utf8proc_url = build_config.dependency_urls[1];  // utf8proc git URL
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "git", "clone", "--depth", "1", "--branch", "v2.10.0",
                      utf8proc_url, "build/vendor/utf8proc");
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to download utf8proc");
            return false;
        }
        
        // Verify the download succeeded by checking for Makefile
        if (nob_get_file_type("build/vendor/utf8proc/Makefile") != NOB_FILE_REGULAR) {
            nob_log(NOB_ERROR, "utf8proc download appears to have failed - no Makefile found");
            return false;
        }
    }
    
    // Always build utf8proc if the static library doesn't exist
    const char *utf8proc_lib_path = build_config.external_dependencies[1];  // "build/vendor/utf8proc/libutf8proc.a"
    if (nob_get_file_type(utf8proc_lib_path) != NOB_FILE_REGULAR) {
        nob_log(NOB_INFO, "Building utf8proc from source...");
        
        const char *old_cwd = nob_get_current_dir_temp();
        if (!nob_set_current_dir("build/vendor/utf8proc")) {
            nob_log(NOB_ERROR, "Could not enter utf8proc directory");
            return false;
        }
        
        Nob_Cmd cmd = {0};
        // Use parallel compilation
        nob_cmd_append(&cmd, "make", "-j");
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to build utf8proc");
            nob_set_current_dir(old_cwd);
            return false;
        }
        
        nob_set_current_dir(old_cwd);
        nob_log(NOB_INFO, "Built utf8proc successfully");
    } else {
        nob_log(NOB_INFO, "utf8proc already built");
    }
    
    return true;
}

bool collect_sources(String_Array *sources) {
    nob_log(NOB_INFO, "Collecting source files...");
    
    // Add main source files from configuration
    for (size_t i = 0; i < build_config.main_sources_count; i++) {
        if (nob_get_file_type(build_config.main_sources[i]) == NOB_FILE_REGULAR) {
            nob_da_append(sources, build_config.main_sources[i]);
        }
    }
    
    // Collect sources from module directories
    for (size_t d = 0; d < build_config.module_directories_count; d++) {
        const char *module_dir = build_config.module_directories[d];
        Nob_File_Paths module_files = {0};
        
        if (nob_read_entire_dir(module_dir, &module_files)) {
            for (size_t i = 0; i < module_files.count; i++) {
                const char *file = module_files.items[i];
                size_t len = strlen(file);
                if (len > 4 && strcmp(file + len - 4, ".cpp") == 0) {
                    // Skip libintl_shim.cpp if scanning utils directory (already included in main_sources)
                    if (strcmp(module_dir, "src/utils") == 0 && strcmp(file, "libintl_shim.cpp") == 0) {
                        continue;
                    }
                    
                    Nob_String_Builder sb = {0};
                    nob_sb_append_cstr(&sb, module_dir);
                    nob_sb_append_cstr(&sb, "/");
                    nob_sb_append_cstr(&sb, file);
                    nob_sb_append_null(&sb);
                    nob_da_append(sources, strdup(sb.items)); // Note: This leaks, but it's build-time only
                    nob_sb_free(sb);
                }
            }
            nob_da_free(module_files);
        }
    }
    
    nob_log(NOB_INFO, "Collected %zu C++ source files", sources->count);
    return true;
}

bool collect_c_sources(String_Array *c_sources) {
    nob_log(NOB_INFO, "Collecting C source files...");
    
    // Add isocline C files from configuration
    for (size_t i = 0; i < build_config.isocline_c_sources_count; i++) {
        if (nob_get_file_type(build_config.isocline_c_sources[i]) == NOB_FILE_REGULAR) {
            nob_da_append(c_sources, build_config.isocline_c_sources[i]);
        }
    }
    
    nob_log(NOB_INFO, "Collected %zu C source files", c_sources->count);
    return true;
}

// Global compiler cache
static const char *cached_cxx_compiler = NULL;
static const char *cached_c_compiler = NULL;
static const char *cached_linker = NULL;

static const char *get_cxx_compiler(void) {
    if (cached_cxx_compiler != NULL) {
        return cached_cxx_compiler;
    }
    
    Nob_Cmd test_cmd = {0};
    nob_cmd_append(&test_cmd, "which", "g++");
    if (nob_cmd_run(&test_cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        cached_cxx_compiler = "g++";
    } else {
        cached_cxx_compiler = "clang++";
    }
    return cached_cxx_compiler;
}

static const char *get_c_compiler(void) {
    if (cached_c_compiler != NULL) {
        return cached_c_compiler;
    }
    
    Nob_Cmd test_cmd = {0};
    nob_cmd_append(&test_cmd, "which", "gcc");
    if (nob_cmd_run(&test_cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        cached_c_compiler = "gcc";
    } else {
        cached_c_compiler = "clang";
    }
    return cached_c_compiler;
}

static const char *get_linker(void) {
    if (cached_linker != NULL) {
        return cached_linker;
    }
    
    Nob_Cmd test_cmd = {0};
    nob_cmd_append(&test_cmd, "which", "g++");
    if (nob_cmd_run(&test_cmd, .stdout_path = "/dev/null", .stderr_path = "/dev/null")) {
        cached_linker = "g++";
    } else {
        cached_linker = "clang++";
    }
    return cached_linker;
}

bool setup_build_flags(Nob_Cmd *cmd) {
    const char *compiler = get_cxx_compiler();
    
    nob_cmd_append(cmd, compiler);
    
    // C++ standard and basic flags
    nob_cmd_append(cmd, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic");
    
    // Platform-specific flags
#ifdef PLATFORM_MACOS
    // Only use libc++ with clang++, g++ uses libstdc++ by default
    if (strcmp(compiler, "clang++") == 0) {
        nob_cmd_append(cmd, "-stdlib=libc++");
    }
#ifdef ARCH_ARM64
    nob_cmd_append(cmd, "-arch", "arm64");
#elif defined(ARCH_X86_64)
    nob_cmd_append(cmd, "-arch", "x86_64");
#endif
#endif

#ifdef PLATFORM_LINUX
    nob_cmd_append(cmd, "-static-libgcc", "-static-libstdc++");
#endif

    // Optimization (use -O2 for performance)
    nob_cmd_append(cmd, "-O2");
    
    // Defines
    nob_cmd_append(cmd, "-DIC_SEPARATE_OBJS=1");
    nob_cmd_append(cmd, "-DJSON_NOEXCEPTION=1");
    nob_cmd_append(cmd, "-DJSON_USE_IMPLICIT_CONVERSIONS=1");
    
    // Include directories from configuration
    for (size_t i = 0; i < build_config.include_directories_count; i++) {
        nob_cmd_append(cmd, "-I", build_config.include_directories[i]);
    }
    
    return true;
}

bool setup_c_build_flags(Nob_Cmd *cmd) {
    const char *c_compiler = get_c_compiler();
    nob_cmd_append(cmd, c_compiler);
    
    // C standard and basic flags
    nob_cmd_append(cmd, "-std=c11", "-Wall", "-Wno-error", "-Wno-unused-function", "-Wno-unused-variable");
    
    // Platform-specific flags
#ifdef PLATFORM_MACOS
#ifdef ARCH_ARM64
    nob_cmd_append(cmd, "-arch", "arm64");
#elif defined(ARCH_X86_64)
    nob_cmd_append(cmd, "-arch", "x86_64");
#endif
#endif

    // Optimization
    nob_cmd_append(cmd, "-O2");
    
    // Defines
    nob_cmd_append(cmd, "-DIC_SEPARATE_OBJS=1");
    
    // Include directories from configuration (C-specific)
    for (size_t i = 0; i < build_config.c_include_directories_count; i++) {
        nob_cmd_append(cmd, "-I", build_config.c_include_directories[i]);
    }
    
    return true;
}

bool compile_cjsh(void) {
    nob_log(NOB_INFO, "Compiling " PROJECT_NAME " (parallel compilation)...");
    
    // Collect source files
    String_Array cpp_sources = {0};
    String_Array c_sources = {0};
    String_Array obj_files = {0};
    
    if (!collect_sources(&cpp_sources)) {
        return false;
    }
    
    if (!collect_c_sources(&c_sources)) {
        return false;
    }
    
    // Initialize parallel process management
    Nob_Procs procs = {0};
    int max_parallel_jobs = nob_nprocs(); // Use CPU core count
    if (max_parallel_jobs <= 0) max_parallel_jobs = 4; // Fallback to 4 if detection fails
    
    nob_log(NOB_INFO, "Using %d parallel compilation jobs", max_parallel_jobs);
    
    // Progress tracking
    size_t completed_cpp_files = 0;
    size_t total_files = cpp_sources.count + c_sources.count;
    
    // Temporarily suppress command logging to reduce build noise
    Nob_Log_Level original_log_level = nob_minimal_log_level;
    nob_minimal_log_level = NOB_WARNING;
    
    // Compile C++ files in parallel
    nob_log(NOB_INFO, "Starting parallel compilation of %zu C++ files...", cpp_sources.count);
    for (size_t i = 0; i < cpp_sources.count; i++) {
        Nob_Cmd cmd = {0};
        if (!setup_build_flags(&cmd)) {
            return false;
        }
        
        // Add compile only flag
        nob_cmd_append(&cmd, "-c");
        
        // Source file
        nob_cmd_append(&cmd, cpp_sources.items[i]);
        
        // Generate object file name
        const char *source = cpp_sources.items[i];
        const char *basename = strrchr(source, '/');
        if (basename) basename++; else basename = source;
        
        Nob_String_Builder obj_name = {0};
        nob_sb_append_cstr(&obj_name, "build/obj/");
        // Replace .cpp with .o
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
        
        // Run asynchronously with automatic process management
        if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_parallel_jobs)) {
            nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
            nob_sb_free(obj_name);
            return false;
        }
        
        // Show fancy progress bar for C++ files
        update_progress("C++ Compilation", i + 1, cpp_sources.count);
        
        nob_sb_free(obj_name);
    }
    
    // Wait for all C++ compilations to complete
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
    nob_log(NOB_INFO, "All %zu C++ files compiled successfully", cpp_sources.count);
    nob_minimal_log_level = NOB_WARNING;
    
    // Compile C files in parallel
    nob_minimal_log_level = original_log_level;
    nob_log(NOB_INFO, "Starting parallel compilation of %zu C files...", c_sources.count);
    nob_minimal_log_level = NOB_WARNING;
    for (size_t i = 0; i < c_sources.count; i++) {
        Nob_Cmd cmd = {0};
        if (!setup_c_build_flags(&cmd)) {
            return false;
        }
        
        // Add compile only flag
        nob_cmd_append(&cmd, "-c");
        
        // Source file
        nob_cmd_append(&cmd, c_sources.items[i]);
        
        // Generate object file name
        const char *source = c_sources.items[i];
        const char *basename = strrchr(source, '/');
        if (basename) basename++; else basename = source;
        
        Nob_String_Builder obj_name = {0};
        nob_sb_append_cstr(&obj_name, "build/obj/");
        // Replace .c with .o
        size_t base_len = strlen(basename);
        if (base_len > 2 && strcmp(basename + base_len - 2, ".c") == 0) {
            nob_sb_append_buf(&obj_name, basename, base_len - 2);
            nob_sb_append_cstr(&obj_name, ".c.o");  // Use .c.o to distinguish from C++ objects
        } else {
            nob_sb_append_cstr(&obj_name, basename);
            nob_sb_append_cstr(&obj_name, ".c.o");
        }
        nob_sb_append_null(&obj_name);
        
        nob_cmd_append(&cmd, "-o", obj_name.items);
        nob_da_append(&obj_files, strdup(obj_name.items));
        
        // Run asynchronously with automatic process management
        if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_parallel_jobs)) {
            nob_log(NOB_ERROR, "Failed to start compilation of %s", source);
            nob_sb_free(obj_name);
            return false;
        }
        
        // Show fancy progress bar for C files
        update_progress("C Compilation   ", i + 1, c_sources.count);
        
        nob_sb_free(obj_name);
    }
    
    // Wait for all C compilations to complete
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
    nob_log(NOB_INFO, "All %zu files compiled successfully!", total_files);
    
    // Restore original log level before linking
    nob_minimal_log_level = original_log_level;
    
    // Link everything together
    nob_log(NOB_INFO, "Linking " PROJECT_NAME "...");
    
    // Temporarily suppress command logging for linking as well
    nob_minimal_log_level = NOB_WARNING;
    
    Nob_Cmd link_cmd = {0};
    
    const char *linker = get_linker();
    nob_cmd_append(&link_cmd, linker);
    
    // Platform-specific linking flags
#ifdef PLATFORM_MACOS
    // Only use libc++ with clang++, g++ uses libstdc++ by default
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
    // Only use static linking with gcc/g++, not with clang
    if (strcmp(linker, "g++") == 0) {
        nob_cmd_append(&link_cmd, "-static-libgcc", "-static-libstdc++");
    }
#endif
    
    // Add all object files
    for (size_t i = 0; i < obj_files.count; i++) {
        nob_cmd_append(&link_cmd, obj_files.items[i]);
    }
    
    // Output file
    nob_cmd_append(&link_cmd, "-o", "build/" PROJECT_NAME);
    
    // Link libraries - pthread and platform-specific standard library
#ifdef PLATFORM_MACOS
    // On macOS, use appropriate standard library based on compiler
    if (strcmp(linker, "clang++") == 0) {
        // clang++ with -stdlib=libc++ handles C++ stdlib automatically
        nob_cmd_append(&link_cmd, "-lpthread");
    } else {
        // g++ uses libstdc++ by default
        nob_cmd_append(&link_cmd, "-lstdc++", "-lpthread");
    }
#else
    nob_cmd_append(&link_cmd, "-lstdc++", "-lpthread");
#endif
    
#if defined(PLATFORM_LINUX) || defined(PLATFORM_UNIX)
    nob_cmd_append(&link_cmd, "-ldl");
#endif
    
    // Link external libraries from configuration
    for (size_t i = 0; i < build_config.external_library_paths_count; i++) {
        nob_cmd_append(&link_cmd, build_config.external_library_paths[i]);
    }
    
    // Execute the linking
    if (!nob_cmd_run(&link_cmd)) {
        nob_minimal_log_level = original_log_level;
        nob_log(NOB_ERROR, "Linking failed");
        return false;
    }
    
    // Restore log level after linking
    nob_minimal_log_level = original_log_level;
    
    // Clean up
    nob_da_free(cpp_sources);
    nob_da_free(c_sources);
    for (size_t i = 0; i < obj_files.count; i++) {
        // Note: We're leaking the strdup'd strings, but this is a build tool that exits
    }
    nob_da_free(obj_files);
    nob_da_free(procs);  // Clean up the process array
    
    return true;
}

void print_help(void) {
    printf("CJ's Shell Build System (nob)\n");
    printf("Usage: nob [OPTIONS]\n\n");
    printf("OPTIONS:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --version     Show version information\n");
    printf("  --clean           Clean build directory\n");
    printf("  --debug           Build with debug symbols\n");
    printf("  --force-32bit     Force 32-bit build (if supported)\n");
    printf("  --dependencies    List project dependencies\n\n");
    printf("Examples:\n");
    printf("  nob                # Build the project\n");
    printf("  nob --clean        # Clean build files\n");
    printf("  nob --debug        # Build with debug info\n");
}

void print_version(void) {
    printf("CJ's Shell Build System\n");
    printf("Project: %s\n", PROJECT_NAME);
    printf("Version: %s\n", VERSION);
    printf("Built with nob.h\n");
}

void print_dependencies(void) {
    printf("CJ's Shell Dependencies\n");
    printf("======================\n\n");
    
    printf("Build Dependencies:\n");
    printf("  - C++ compiler (g++ or clang++)\n");
    printf("  - C compiler (gcc or clang)\n");
    printf("  - make (for building utf8proc)\n");
    printf("  - git (for downloading dependencies)\n");
    printf("  - curl or wget (for downloading files)\n\n");
    
    printf("Runtime Dependencies (automatically downloaded):\n");
    for (size_t i = 0; i < build_config.external_dependencies_count; i++) {
        const char *dep = build_config.external_dependencies[i];
        if (strstr(dep, "json.hpp")) {
            printf("  - nlohmann/json v3.11.3 (JSON parsing library)\n");
            printf("    URL: https://github.com/nlohmann/json\n");
        } else if (strstr(dep, "utf8proc")) {
            printf("  - utf8proc v2.10.0 (Unicode text processing library)\n");
            printf("    URL: https://github.com/JuliaStrings/utf8proc\n");
        }
    }
    
    printf("\nSystem Libraries (linked at build time):\n");
    printf("  - pthread (POSIX threads)\n");
    printf("  - C++ standard library\n");
#if defined(PLATFORM_LINUX) || defined(PLATFORM_UNIX)
    printf("  - dl (dynamic loading)\n");
#endif
    
    printf("\nNote: This build system downloads and builds all external\n");
    printf("dependencies from source for maximum compatibility.\n");
    printf("No system package manager dependencies are required.\n");
}

bool create_required_directories(void) {
    nob_log(NOB_INFO, "Creating required directories...");
    
    // Create all required directories from configuration
    for (size_t i = 0; i < build_config.required_directories_count; i++) {
        if (!nob_mkdir_if_not_exists(build_config.required_directories[i])) {
            nob_log(NOB_ERROR, "Could not create directory: %s", build_config.required_directories[i]);
            return false;
        }
        nob_log(NOB_INFO, "Created directory: %s", build_config.required_directories[i]);
    }
    
    return true;
}