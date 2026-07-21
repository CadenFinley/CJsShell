# CMakeLists.txt
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set(
    CJSH_ISOCLINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/isocline.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/attr.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/bbcode.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/common.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/completion/completers.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/completion/completions.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/edit/editline.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/completion/highlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/edit/history.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/fuzzy_match.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/isocline_env.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/keybinding/isocline_keybindings.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/isocline_options.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/isocline_print.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/edit/isocline_readline.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/isocline_terminal.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/edit/isocline_typeahead.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/edit/prompt_line_replacement.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/stringbuf.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/term.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/tty.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/tty_esc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/edit/undo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/unicode.c"
)

file(
    GLOB_RECURSE CJSH_ISOCLINE_HEADERS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

cjsh_collect_include_dirs(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src" OUT_VAR CJSH_ISOCLINE_INCLUDE_DIRS)

set(CJSH_ISOCLINE_COMPAT_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${CJSH_ISOCLINE_COMPAT_INCLUDE_DIR}/isocline")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/isocline.h"
    "${CJSH_ISOCLINE_COMPAT_INCLUDE_DIR}/isocline/isocline.h"
    COPYONLY
)

add_library(cjsh_isocline STATIC)
target_sources(cjsh_isocline PRIVATE ${CJSH_ISOCLINE_SOURCES} ${CJSH_ISOCLINE_HEADERS})
target_include_directories(
    cjsh_isocline
    PUBLIC
        ${CJSH_ISOCLINE_INCLUDE_DIRS}
        ${CJSH_ISOCLINE_COMPAT_INCLUDE_DIR}
)
target_link_libraries(cjsh_isocline PUBLIC cjsh_build_settings Threads::Threads)
target_compile_definitions(cjsh_isocline PUBLIC IC_SEPARATE_OBJS=1)
set_target_properties(
    cjsh_isocline
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
)
