cmake_minimum_required(VERSION 3.10)
project(fast_prompt_tags)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add option for forcing 32-bit build (to match main project)
option(FORCE_32BIT "Force 32-bit build" OFF)

# Handle architecture detection (passed from build script or detected)
if(NOT DEFINED ARCH)
    if(FORCE_32BIT)
        set(ARCH "x86")
    else()
        # Fallback architecture detection
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(ARCH "arm64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
            set(ARCH "x86_64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i386|i686|x86)$")
            set(ARCH "x86")
        else()
            set(ARCH "x86_64")  # Default fallback
        endif()
    endif()
endif()

message(STATUS "Plugin building for architecture: ${ARCH}")

# Set architecture-specific compiler flags
if(ARCH STREQUAL "x86")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    message(STATUS "Enabled 32-bit compilation flags")
elseif(ARCH STREQUAL "arm64")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch arm64")
    message(STATUS "Enabled ARM64 compilation flags")
elseif(ARCH STREQUAL "x86_64")
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch x86_64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch x86_64")
    message(STATUS "Enabled x86_64 compilation flags")
endif()

# Add threading support
find_package(Threads REQUIRED)

# Get the absolute path to the main project include directory
get_filename_component(MAIN_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)

# Define the source files
set(SOURCES
    fast_prompt_tags.cpp
)

# Create a shared library
add_library(fast_prompt_tags SHARED ${SOURCES})

# Add include directories
target_include_directories(fast_prompt_tags PRIVATE ${MAIN_PROJECT_DIR}/include/plugins)

# Link with the thread library
target_link_libraries(fast_prompt_tags PRIVATE Threads::Threads)

# Set architecture-specific optimizations (conservative approach)
if(ARCH STREQUAL "arm64" AND APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    # Apple Silicon optimizations - only when actually building on ARM64
    target_compile_options(fast_prompt_tags PRIVATE -mcpu=apple-a14)
elseif(ARCH STREQUAL "x86_64" AND NOT APPLE)
    # x86_64 optimizations for non-Apple platforms only
    target_compile_options(fast_prompt_tags PRIVATE -march=x86-64 -mtune=generic)
elseif(ARCH STREQUAL "x86" AND NOT APPLE)
    # 32-bit x86 optimizations for non-Apple platforms only
    target_compile_options(fast_prompt_tags PRIVATE -march=i686 -mtune=generic)
endif()

# Use general optimization flags that work across platforms
target_compile_options(fast_prompt_tags PRIVATE -O2)

# Set output directory and proper extension/flags
if(APPLE)
    set_target_properties(fast_prompt_tags PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        SUFFIX ".dylib"
        PREFIX ""
        # Permit undefined symbols (from host application) during linking
        LINK_FLAGS "-undefined dynamic_lookup -flat_namespace"
        OUTPUT_NAME "fast_prompt_tags"
    )
else()
    set_target_properties(fast_prompt_tags PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        SUFFIX ".so"
        PREFIX ""
        OUTPUT_NAME "fast_prompt_tags"
    )
endif()

# Installation
install(TARGETS fast_prompt_tags
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)
