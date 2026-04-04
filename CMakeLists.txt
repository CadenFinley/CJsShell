cmake_minimum_required(VERSION 3.25)

project(cjsh VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${_cfg}" _cfg_upper)
        set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_upper}" "${CMAKE_BINARY_DIR}")
    endforeach()
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        Release
        CACHE STRING "Build type" FORCE
    )
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

set(_cjsh_strip_binary_default ON)
if(DEFINED ENV{CJSH_STRIP_BINARY})
    string(TOUPPER "$ENV{CJSH_STRIP_BINARY}" _cjsh_strip_binary_env)
    if(_cjsh_strip_binary_env STREQUAL "0"
       OR _cjsh_strip_binary_env STREQUAL "OFF"
       OR _cjsh_strip_binary_env STREQUAL "FALSE"
       OR _cjsh_strip_binary_env STREQUAL "NO")
        set(_cjsh_strip_binary_default OFF)
    endif()
endif()

option(CJSH_GENERATE_COMPILE_COMMANDS "Generate compile_commands.json" ON)
option(CJSH_BUILD_TESTS "Build cjsh test binaries" ON)
option(CJSH_MINIMAL_BUILD "Enable the minimal size-oriented build profile" OFF)
option(CJSH_STRIP_BINARY "Strip non-Debug binaries" ${_cjsh_strip_binary_default})

set(CMAKE_EXPORT_COMPILE_COMMANDS ${CJSH_GENERATE_COMPILE_COMMANDS})

include(CTest)
if(CJSH_BUILD_TESTS AND NOT BUILD_TESTING)
    set(BUILD_TESTING
        ON
        CACHE BOOL "Build tests" FORCE
    )
endif()
if(BUILD_TESTING)
    enable_testing()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CJSHBuildMetadata)
include(CJSHBuildSettings)

find_package(Threads REQUIRED)

cjsh_compute_build_metadata(
    OUT_GIT_HASH CJSH_METADATA_GIT_HASH
    OUT_GIT_HASH_FULL CJSH_METADATA_GIT_HASH_FULL
    OUT_BUILD_ARCH CJSH_METADATA_BUILD_ARCH
    OUT_BUILD_PLATFORM CJSH_METADATA_BUILD_PLATFORM
    OUT_BUILD_TIME CJSH_METADATA_BUILD_TIME
    OUT_BUILD_COMPILER CJSH_METADATA_BUILD_COMPILER
    OUT_CXX_STANDARD CJSH_METADATA_CXX_STANDARD
    OUT_BUILD_TYPE CJSH_METADATA_BUILD_TYPE
)

add_library(cjsh_build_metadata INTERFACE)
cjsh_apply_metadata_definitions(
    TARGET cjsh_build_metadata
    GIT_HASH "${CJSH_METADATA_GIT_HASH}"
    GIT_HASH_FULL "${CJSH_METADATA_GIT_HASH_FULL}"
    BUILD_ARCH "${CJSH_METADATA_BUILD_ARCH}"
    BUILD_PLATFORM "${CJSH_METADATA_BUILD_PLATFORM}"
    BUILD_TIME "${CJSH_METADATA_BUILD_TIME}"
    BUILD_COMPILER "${CJSH_METADATA_BUILD_COMPILER}"
    CXX_STANDARD "${CJSH_METADATA_CXX_STANDARD}"
    BUILD_TYPE "${CJSH_METADATA_BUILD_TYPE}"
)

add_library(cjsh_build_profile INTERFACE)
cjsh_apply_build_profile(
    TARGET cjsh_build_profile
    MINIMAL_BUILD ${CJSH_MINIMAL_BUILD}
)

add_library(cjsh_build_settings INTERFACE)
target_link_libraries(cjsh_build_settings INTERFACE cjsh_build_metadata cjsh_build_profile)

add_subdirectory(cjsh-isocline "${CMAKE_BINARY_DIR}/modules/cjsh-isocline")
add_subdirectory(cjsh "${CMAKE_BINARY_DIR}/modules/cjsh")

if(CJSH_BUILD_TESTS)
    add_subdirectory(tests "${CMAKE_BINARY_DIR}/modules/tests")
endif()
