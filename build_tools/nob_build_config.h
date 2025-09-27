#ifndef CJSH_NOB_BUILD_CONFIG_H
#define CJSH_NOB_BUILD_CONFIG_H

#include "nob_types.h"

static const Build_Config build_config = {
    .main_sources =
        (const char*[]){"src/cjsh.cpp", "src/error_out.cpp", "src/exec.cpp",
                        "src/job_control.cpp", "src/main_loop.cpp",
                        "src/shell.cpp", "src/signal_handler.cpp",
                        "src/utils/libintl_shim.cpp"},
    .main_sources_count = 8,

    .module_directories =
        (const char*[]){"src/builtin", "src/ai", "src/prompt",
                        "src/prompt/modules", "src/script_interpreter",
                        "src/plugins", "src/utils"},
    .module_directories_count = 7,

    .isocline_c_sources =
        (const char*[]){"src/isocline/attr.c", "src/isocline/bbcode.c",
                        "src/isocline/bbcode_colors.c", "src/isocline/common.c",
                        "src/isocline/completers.c",
                        "src/isocline/completions.c", "src/isocline/editline.c",
                        "src/isocline/highlight.c", "src/isocline/history.c",
                        "src/isocline/isocline.c", "src/isocline/stringbuf.c",
                        "src/isocline/term.c", "src/isocline/tty.c",
                        "src/isocline/tty_esc.c", "src/isocline/undo.c"},
    .isocline_c_sources_count = 15,

    .include_directories =
        (const char*[]){"include", "include/isocline", "include/builtin",
                        "include/utils", "include/prompt",
                        "include/prompt/modules", "include/ai",
                        "include/plugins", "include/script_interpreter",
                        "build/vendor", "build/vendor/utf8proc"},
    .include_directories_count = 11,

    .c_include_directories =
        (const char*[]){"include", "include/isocline", "build/vendor/utf8proc"},
    .c_include_directories_count = 3,

    .required_directories =
        (const char*[]){"build", "build/obj", "build/vendor",
                        "build/vendor/nlohmann", "build/vendor/utf8proc"},
    .required_directories_count = 5,

    .external_dependencies =
        (const char*[]){"build/vendor/nlohmann/json.hpp",
                        "build/vendor/utf8proc/libutf8proc.a"},
    .external_dependencies_count = 2,

    .external_library_paths =
        (const char*[]){"build/vendor/utf8proc/libutf8proc.a"},
    .external_library_paths_count = 1,

    .dependency_urls =
        (const char*[]){"https://github.com/nlohmann/json/releases/download/"
                        "v3.11.3/json.hpp",
                        "https://github.com/JuliaStrings/utf8proc.git"},
    .dependency_urls_count = 2};

#endif  // CJSH_NOB_BUILD_CONFIG_H
