cmake_minimum_required(VERSION 3.16)

project(isocline VERSION 1.0.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ISOCLINE_TINY_BUILD "Optimize the core library for minimal size" OFF)
option(ISOCLINE_MONOLITHIC "Build the core library from a single translation unit" ON)

if(ISOCLINE_MONOLITHIC)
    set(ISOCLINE_SOURCES
        src/isocline.c
    )
else()
    set(ISOCLINE_SOURCES
        src/attr.c
        src/bbcode.c
        src/common.c
        src/completers.c
        src/completions.c
        src/editline.c
        src/highlight.c
        src/history.c
        src/isocline_env.c
        src/isocline_keybindings.c
        src/isocline_options.c
        src/isocline_print.c
        src/isocline_readline.c
        src/isocline_terminal.c
        src/stringbuf.c
        src/term.c
        src/tty.c
        src/tty_esc.c
        src/undo.c
        src/unicode.c
    )
endif()

add_library(isocline ${ISOCLINE_SOURCES})
add_library(isocline::isocline ALIAS isocline)

target_compile_features(isocline PUBLIC c_std_11)

target_compile_options(isocline PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(isocline
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(isocline
    PRIVATE
        _CRT_NONSTDC_NO_WARNINGS
        _CRT_SECURE_NO_WARNINGS
        _DEFAULT_SOURCE
        _XOPEN_SOURCE=700
)

if(NOT ISOCLINE_MONOLITHIC)
    target_compile_definitions(isocline PRIVATE IC_SEPARATE_OBJS=1)
endif()

if(ISOCLINE_TINY_BUILD)
    target_compile_definitions(isocline PUBLIC ISOCLINE_TINY_BUILD=1)
endif()

foreach(_iso_opt -O0 -g -fno-omit-frame-pointer)
    target_compile_options(isocline PRIVATE $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C>>:${_iso_opt}>)
endforeach()

set(_iso_release_opts
    -ffunction-sections
    -fdata-sections
    -fomit-frame-pointer
    -fmerge-all-constants
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
    list(APPEND _iso_release_opts -Oz)
else()
    list(APPEND _iso_release_opts -Os)
endif()

if(ISOCLINE_TINY_BUILD)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
        list(APPEND _iso_release_opts -fno-unwind-tables -fno-asynchronous-unwind-tables)
        list(APPEND _iso_release_opts -fno-stack-protector)
    endif()
endif()

foreach(_iso_opt IN LISTS _iso_release_opts)
    target_compile_options(isocline PRIVATE
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<COMPILE_LANGUAGE:C>>:${_iso_opt}>
    )
endforeach()

include(CheckIPOSupported)
check_ipo_supported(RESULT isocline_ipo_supported OUTPUT isocline_ipo_output)
if(isocline_ipo_supported)
    set_property(TARGET isocline PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET isocline PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
else()
    message(STATUS "IPO/LTO not supported: ${isocline_ipo_output}")
endif()

set_target_properties(isocline PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME isocline
)

install(TARGETS isocline
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})