cmake_minimum_required(VERSION 3.25)

project(cjsh VERSION 3.11.6 LANGUAGES C CXX)

include(GNUInstallDirs)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

option(CJSH_MINIMAL_BUILD "Enable ultra-small minimal build profile" OFF)

set(_CJSH_STRIP_DEFAULT ON)
if(DEFINED ENV{CJSH_STRIP_BINARY} AND "$ENV{CJSH_STRIP_BINARY}" STREQUAL "0")
    set(_CJSH_STRIP_DEFAULT OFF)
endif()
option(CJSH_STRIP_BINARY "Strip symbols from the cjsh binary in non-Debug builds" ${_CJSH_STRIP_DEFAULT})

option(CJSH_GENERATE_COMPILE_COMMANDS "Emit compile_commands.json during configuration" ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ${CJSH_GENERATE_COMPILE_COMMANDS} CACHE BOOL "Export compilation database" FORCE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG" CACHE STRING "Release C compiler flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG" CACHE STRING "Release CXX compiler flags" FORCE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${_cfg} _cfg_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_upper} ${CMAKE_BINARY_DIR})
    endforeach()
endif()

define_property(DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS
    BRIEF_DOCS "Files that trigger CMake reconfigure"
    FULL_DOCS "Files watched to trigger reconfiguration when their timestamps change.")
set(_git_head_file "${CMAKE_SOURCE_DIR}/.git/HEAD")
if(EXISTS "${_git_head_file}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_git_head_file}")
    file(READ "${_git_head_file}" _git_head_content)
    string(REGEX MATCH "ref: (.+)" _git_head_match "${_git_head_content}")
    if(CMAKE_MATCH_1)
        set(_git_ref "${CMAKE_SOURCE_DIR}/.git/${CMAKE_MATCH_1}")
        if(EXISTS "${_git_ref}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_git_ref}")
        endif()
    endif()
endif()

set(CJSH_GIT_HASH "unknown")
if(DEFINED ENV{CJSH_GIT_HASH_OVERRIDE} AND NOT "$ENV{CJSH_GIT_HASH_OVERRIDE}" STREQUAL "")
    set(CJSH_GIT_HASH "$ENV{CJSH_GIT_HASH_OVERRIDE}")
else()
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _git_rev
            RESULT_VARIABLE _git_rev_result
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_git_rev_result EQUAL 0 AND NOT _git_rev STREQUAL "")
            set(CJSH_GIT_HASH "${_git_rev}")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} status --porcelain
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE _git_status
                RESULT_VARIABLE _git_status_result
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_git_status_result EQUAL 0 AND NOT _git_status STREQUAL "")
                set(CJSH_GIT_HASH "${CJSH_GIT_HASH}-dirty")
            endif()
        endif()
    endif()
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cjsh_proc)
if(_cjsh_proc MATCHES "^(aarch64|arm64)$")
    set(CJSH_BUILD_ARCH "arm64")
elseif(_cjsh_proc MATCHES "^(x86_64|amd64)$")
    set(CJSH_BUILD_ARCH "x86_64")
elseif(_cjsh_proc MATCHES "^(i[3-6]86|x86)$")
    set(CJSH_BUILD_ARCH "x86")
elseif(_cjsh_proc MATCHES "(ppc|power)")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CJSH_BUILD_ARCH "ppc")
    else()
        set(CJSH_BUILD_ARCH "ppc64")
    endif()
else()
    set(CJSH_BUILD_ARCH "unknown")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(CJSH_BUILD_PLATFORM "apple-darwin")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CJSH_BUILD_PLATFORM "linux")
elseif(WIN32)
    set(CJSH_BUILD_PLATFORM "windows")
else()
    set(CJSH_BUILD_PLATFORM "unix")
endif()

file(GLOB_RECURSE CJSH_CPP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

set(CJSH_C_SOURCES
    ${CMAKE_SOURCE_DIR}/src/isocline/attr.c
    ${CMAKE_SOURCE_DIR}/src/isocline/bbcode.c
    ${CMAKE_SOURCE_DIR}/src/isocline/bbcode_colors.c
    ${CMAKE_SOURCE_DIR}/src/isocline/common.c
    ${CMAKE_SOURCE_DIR}/src/isocline/completers.c
    ${CMAKE_SOURCE_DIR}/src/isocline/completions.c
    ${CMAKE_SOURCE_DIR}/src/isocline/editline.c
    ${CMAKE_SOURCE_DIR}/src/isocline/highlight.c
    ${CMAKE_SOURCE_DIR}/src/isocline/history.c
    ${CMAKE_SOURCE_DIR}/src/isocline/isocline.c
    ${CMAKE_SOURCE_DIR}/src/isocline/isocline_env.c
    ${CMAKE_SOURCE_DIR}/src/isocline/isocline_keybindings.c
    ${CMAKE_SOURCE_DIR}/src/isocline/isocline_options.c
    ${CMAKE_SOURCE_DIR}/src/isocline/isocline_print.c
    ${CMAKE_SOURCE_DIR}/src/isocline/isocline_readline.c
    ${CMAKE_SOURCE_DIR}/src/isocline/isocline_terminal.c
    ${CMAKE_SOURCE_DIR}/src/isocline/stringbuf.c
    ${CMAKE_SOURCE_DIR}/src/isocline/term.c
    ${CMAKE_SOURCE_DIR}/src/isocline/tty.c
    ${CMAKE_SOURCE_DIR}/src/isocline/tty_esc.c
    ${CMAKE_SOURCE_DIR}/src/isocline/undo.c
    ${CMAKE_SOURCE_DIR}/src/isocline/unicode.c
)

set(CJSH_SOURCES ${CJSH_CPP_SOURCES} ${CJSH_C_SOURCES})
list(REMOVE_DUPLICATES CJSH_SOURCES)
list(SORT CJSH_SOURCES)

if(NOT CJSH_SOURCES)
    message(FATAL_ERROR "No source files located under src/. Check the source tree.")
endif()

add_executable(cjsh ${CJSH_SOURCES})

target_compile_features(cjsh PRIVATE cxx_std_17)

set(CJSH_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/isocline
    ${CMAKE_SOURCE_DIR}/include/builtin
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/prompt
    ${CMAKE_SOURCE_DIR}/include/prompt/modules
    ${CMAKE_SOURCE_DIR}/include/interpreter
    ${CMAKE_SOURCE_DIR}/include/parser
    ${CMAKE_SOURCE_DIR}/include/unicode
    ${CMAKE_SOURCE_DIR}/include/syntax_highlighter
    ${CMAKE_SOURCE_DIR}/include/completions
    ${CMAKE_SOURCE_DIR}/include/builtin/cjshopt
)

target_include_directories(cjsh PRIVATE ${CJSH_INCLUDE_DIRS})

target_compile_definitions(cjsh PRIVATE
    CJSH_GIT_HASH="${CJSH_GIT_HASH}"
    CJSH_BUILD_ARCH="${CJSH_BUILD_ARCH}"
    CJSH_BUILD_PLATFORM="${CJSH_BUILD_PLATFORM}"
    IC_SEPARATE_OBJS=1
    JSON_NOEXCEPTION=1
    JSON_USE_IMPLICIT_CONVERSIONS=1
    $<$<CONFIG:Debug>:DEBUG;CJSH_ENABLE_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:IC_NO_DEBUG_MSG=1>
    $<$<BOOL:${CJSH_MINIMAL_BUILD}>:CJSH_MINIMAL_BUILD=1;CJSH_NO_FANCY_FEATURES=1>
    $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>>:_FORTIFY_SOURCE=1>
    $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${CJSH_MINIMAL_BUILD}>>>:_FORTIFY_SOURCE=1>
)

foreach(_cjsh_opt -Wall -Wextra -Wpedantic)
    target_compile_options(cjsh PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${_cjsh_opt}>)
endforeach()

foreach(_cjsh_opt -Wall -Wno-error -Wno-unused-function -Wno-unused-variable)
    target_compile_options(cjsh PRIVATE $<$<COMPILE_LANGUAGE:C>:${_cjsh_opt}>)
endforeach()

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cjsh PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
endif()

if(CJSH_MINIMAL_BUILD)
    message(STATUS "CJSH minimal build profile enabled")
endif()

if(CJSH_MINIMAL_BUILD AND CMAKE_CONFIGURATION_TYPES AND "Debug" IN_LIST CMAKE_CONFIGURATION_TYPES)
    message(STATUS "CJSH minimal build flags apply to all non-Debug configurations")
endif()

set(CJSH_LTO_FLAG "-flto" CACHE STRING "Link-time optimisation flag applied to non-Debug builds")
set(_cjsh_lto_flag "${CJSH_LTO_FLAG}")

set(_cjsh_release_arch_opts)
set(_cjsh_minimal_arch_opts)
if(CJSH_BUILD_ARCH STREQUAL "arm64")
    set(_cjsh_release_arch_opts -mcpu=apple-a14)
    set(_cjsh_minimal_arch_opts -mcpu=apple-a14)
elseif(CJSH_BUILD_ARCH STREQUAL "x86_64")
    set(_cjsh_release_arch_opts -march=x86-64 -mtune=generic)
    set(_cjsh_minimal_arch_opts -march=x86-64 -mtune=generic -msse2 -mfpmath=sse)
elseif(CJSH_BUILD_ARCH STREQUAL "ppc64")
    set(_cjsh_release_arch_opts -mcpu=native -mtune=native)
    set(_cjsh_minimal_arch_opts -mcpu=native -mtune=native)
elseif(CJSH_BUILD_ARCH STREQUAL "ppc")
    # Usage of -mcpu with 32-bit ppc can produce broken code when cpu is 64-bit-capable
    set(_cjsh_release_arch_opts -mtune=native)
    set(_cjsh_minimal_arch_opts -mtune=native)
endif()

foreach(_cjsh_opt -O0 -g -fno-omit-frame-pointer)
    target_compile_options(cjsh PRIVATE $<$<CONFIG:Debug>:${_cjsh_opt}>)
endforeach()

target_compile_options(cjsh PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>>:-fsanitize=address>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C>>:-fsanitize=address>
)

target_link_options(cjsh PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)

set(_cjsh_release_cxx_opts
    -O2
    -ffunction-sections
    -fdata-sections
    ${_cjsh_lto_flag}
    -fomit-frame-pointer
    -fmerge-all-constants
    -fno-rtti
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -U_FORTIFY_SOURCE
)

foreach(_cjsh_opt IN LISTS _cjsh_release_cxx_opts)
    target_compile_options(cjsh PRIVATE
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${CJSH_MINIMAL_BUILD}>>,$<COMPILE_LANGUAGE:CXX>>:${_cjsh_opt}>
    )
endforeach()

set(_cjsh_release_c_opts
    -O2
    -ffunction-sections
    -fdata-sections
    ${_cjsh_lto_flag}
    -fomit-frame-pointer
    -fmerge-all-constants
    -fvisibility=hidden
    -U_FORTIFY_SOURCE
)

foreach(_cjsh_opt IN LISTS _cjsh_release_c_opts)
    target_compile_options(cjsh PRIVATE
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${CJSH_MINIMAL_BUILD}>>,$<COMPILE_LANGUAGE:C>>:${_cjsh_opt}>
    )
endforeach()

if(_cjsh_release_arch_opts)
    foreach(_cjsh_opt IN LISTS _cjsh_release_arch_opts)
        target_compile_options(cjsh PRIVATE
            $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${CJSH_MINIMAL_BUILD}>>,$<COMPILE_LANGUAGE:CXX>>:${_cjsh_opt}>
        )
        target_compile_options(cjsh PRIVATE
            $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${CJSH_MINIMAL_BUILD}>>,$<COMPILE_LANGUAGE:C>>:${_cjsh_opt}>
        )
    endforeach()
endif()

set(_cjsh_minimal_cxx_opts
    -Oz
    -ffunction-sections
    -fdata-sections
    ${_cjsh_lto_flag}
    -fomit-frame-pointer
    -fmerge-all-constants
    -fno-rtti
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -ftemplate-depth=64
    -fno-threadsafe-statics
    -U_FORTIFY_SOURCE
)

foreach(_cjsh_opt IN LISTS _cjsh_minimal_cxx_opts)
    target_compile_options(cjsh PRIVATE
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>,$<COMPILE_LANGUAGE:CXX>>:${_cjsh_opt}>
    )
endforeach()

set(_cjsh_minimal_c_opts
    -Oz
    -ffunction-sections
    -fdata-sections
    ${_cjsh_lto_flag}
    -fomit-frame-pointer
    -fmerge-all-constants
    -fvisibility=hidden
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -U_FORTIFY_SOURCE
)

foreach(_cjsh_opt IN LISTS _cjsh_minimal_c_opts)
    target_compile_options(cjsh PRIVATE
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>,$<COMPILE_LANGUAGE:C>>:${_cjsh_opt}>
    )
endforeach()

if(_cjsh_minimal_arch_opts)
    foreach(_cjsh_opt IN LISTS _cjsh_minimal_arch_opts)
        target_compile_options(cjsh PRIVATE
            $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>,$<COMPILE_LANGUAGE:CXX>>:${_cjsh_opt}>
        )
        target_compile_options(cjsh PRIVATE
            $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>,$<COMPILE_LANGUAGE:C>>:${_cjsh_opt}>
        )
    endforeach()
endif()

target_link_options(cjsh PRIVATE
    $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>>:${_cjsh_lto_flag}>
    $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${CJSH_MINIMAL_BUILD}>>>:${_cjsh_lto_flag}>
)

if(APPLE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(cjsh PRIVATE -stdlib=libc++)
    endif()
    target_link_options(cjsh PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip -Wl,-dead_strip_dylibs>
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>>:-Wl,-no_compact_unwind -Wl,-no_function_starts>
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(cjsh PRIVATE $<$<NOT:$<CONFIG:Debug>>:-static-libgcc -static-libstdc++>)
    endif()
    target_link_options(cjsh PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:-Wl,--gc-sections -Wl,--as-needed>
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${CJSH_MINIMAL_BUILD}>>:-Wl,--strip-all -Wl,--discard-all -Wl,--no-undefined -Wl,--compress-debug-sections=zlib -Wl,-O2 -Wl,--hash-style=gnu>
    )
endif()

find_package(Threads REQUIRED)
target_link_libraries(cjsh PRIVATE Threads::Threads)

if(CMAKE_DL_LIBS)
    target_link_libraries(cjsh PRIVATE ${CMAKE_DL_LIBS})
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <atomic>
    int main() {
        std::atomic<uint64_t> w{0};
        return ++w;
    }
" CJSH_BUILDS_WITHOUT_LIBATOMIC)

if(NOT CJSH_BUILDS_WITHOUT_LIBATOMIC)
    target_link_libraries(cjsh PRIVATE atomic)
endif()

if(CJSH_STRIP_BINARY)
    find_program(CJSH_STRIP_TOOL strip)
    if(CJSH_STRIP_TOOL)
        if(APPLE)
            set(_cjsh_strip_flags -x)
        elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
            set(_cjsh_strip_flags --strip-unneeded)
        else()
            set(_cjsh_strip_flags)
        endif()
        if(CMAKE_CONFIGURATION_TYPES)
            set(_cjsh_strip_configs)
            foreach(_cjsh_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
                if(NOT _cjsh_cfg STREQUAL "Debug")
                    list(APPEND _cjsh_strip_configs ${_cjsh_cfg})
                endif()
            endforeach()
            if(_cjsh_strip_configs)
                add_custom_command(TARGET cjsh POST_BUILD
                    COMMAND ${CJSH_STRIP_TOOL} ${_cjsh_strip_flags} $<TARGET_FILE:cjsh>
                    COMMAND_EXPAND_LISTS
                    COMMENT "Stripping symbols from cjsh"
                    VERBATIM
                    CONFIGURATIONS ${_cjsh_strip_configs})
            endif()
        elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_custom_command(TARGET cjsh POST_BUILD
                COMMAND ${CJSH_STRIP_TOOL} ${_cjsh_strip_flags} $<TARGET_FILE:cjsh>
                COMMAND_EXPAND_LISTS
                COMMENT "Stripping symbols from cjsh"
                VERBATIM)
        endif()
    else()
        message(WARNING "strip tool not found; CJSH_STRIP_BINARY option will be ignored")
    endif()
endif()

install(TARGETS cjsh RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
