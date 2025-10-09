#ifndef CJSH_NOB_BUILD_CONFIG_H
#define CJSH_NOB_BUILD_CONFIG_H

#include "nob_types.h"

static const Build_Config build_config = {
    .main_sources = (const char*[]){"src/cjsh.cpp", "src/error_out.cpp", "src/exec.cpp",
                                    "src/job_control.cpp", "src/main_loop.cpp", "src/shell.cpp",
                                    "src/signal_handler.cpp", "src/utils/libintl_shim.cpp",
                                    "src/shell_env.cpp", "src/flags.cpp", "src/typeahead.cpp"},
    .main_sources_count = 10,

    .module_directories =
        (const char*[]){"src/builtin", "src/prompt", "src/prompt/modules", "src/interpreter",
                        "src/utils", "src/parser", "src/unicode"},
    .module_directories_count = 7,

    .isocline_c_sources = (const char*[]){"src/isocline/attr.c",
                                          "src/isocline/bbcode.c",
                                          "src/isocline/bbcode_colors.c",
                                          "src/isocline/common.c",
                                          "src/isocline/completers.c",
                                          "src/isocline/completions.c",
                                          "src/isocline/editline.c",
                                          "src/isocline/highlight.c",
                                          "src/isocline/history.c",
                                          "src/isocline/isocline.c",
                                          "src/isocline/isocline_env.c",
                                          "src/isocline/isocline_keybindings.c",
                                          "src/isocline/isocline_options.c",
                                          "src/isocline/isocline_print.c",
                                          "src/isocline/isocline_readline.c",
                                          "src/isocline/isocline_terminal.c",
                                          "src/isocline/stringbuf.c",
                                          "src/isocline/term.c",
                                          "src/isocline/tty.c",
                                          "src/isocline/tty_esc.c",
                                          "src/isocline/undo.c"},
    .isocline_c_sources_count = 21,

    .include_directories =
        (const char*[]){"include", "include/isocline", "include/builtin", "include/utils",
                        "include/prompt", "include/prompt/modules", "include/interpreter",
                        "include/parser", "include/unicode"},
    .include_directories_count = 9,

    .c_include_directories = (const char*[]){"include", "include/isocline"},
    .c_include_directories_count = 2,

    .required_directories = (const char*[]){"build", "build/obj"},
    .required_directories_count = 2,

    .external_dependencies = NULL,
    .external_dependencies_count = 0,

    .external_library_paths = NULL,
    .external_library_paths_count = 0,

    .dependency_urls = NULL,
    .dependency_urls_count = 0};

#endif
