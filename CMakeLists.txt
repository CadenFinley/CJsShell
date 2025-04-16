cmake_minimum_required(VERSION 3.10)

# Check for Unix platform - fail on Windows
if(WIN32)
    message(FATAL_ERROR "DevToolsTerminal is currently only supported on Unix-based systems (Linux/macOS)")
endif()

# Set the project name
project(DevToolsTerminal)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Platform-specific configurations
if(APPLE)
    set(MACOS_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/macos)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")
endif()

# Linux build configuration
set(LINUX_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17) # Ensure C++17 standard is used
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include the header files directory
include_directories(src/include)

# Add ExternalProject support
include(ExternalProject)

# Function to set up CURL download and build
function(setup_curl_download)
    message(STATUS "CURL not found - will download and build from source")
    ExternalProject_Add(curl_external
        URL https://curl.se/download/curl-8.1.2.tar.gz
        PREFIX ${CMAKE_BINARY_DIR}/external/curl
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external/curl/install
            -DBUILD_SHARED_LIBS=OFF
            -DCMAKE_USE_OPENSSL=ON
            -DBUILD_TESTING=OFF
        )
    set(CURL_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/external/curl/install/include PARENT_SCOPE)
    set(CURL_LIBRARIES ${CMAKE_BINARY_DIR}/external/curl/install/lib/${CMAKE_STATIC_LIBRARY_PREFIX}curl${CMAKE_STATIC_LIBRARY_SUFFIX} PARENT_SCOPE)
    add_library(CURL::libcurl UNKNOWN IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION ${CURL_LIBRARIES}
        INTERFACE_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIRS})
    add_dependencies(CURL::libcurl curl_external)
endfunction()

# Try to find CURL, download if not found
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    setup_curl_download()
endif()

find_package(Threads REQUIRED)  # Add this to find pthread library

# Source files
set(SOURCE_FILES
    src/s_main.cpp
    src/terminalpassthrough.cpp
    src/openaipromptengine.cpp
    src/pluginmanager.cpp
    src/thememanager.cpp
    src/daemonmanager.cpp
)

# Add the include directories for CURL
include_directories(${CURL_INCLUDE_DIRS})

# Find nlohmann_json package (Unix systems only)
find_path(JSON_INCLUDE_DIR NAMES nlohmann/json.hpp
    PATHS
    /usr/include
    /usr/local/include
    /opt/include
    /opt/local/include
    ${CMAKE_SOURCE_DIR}/external
)

if(JSON_INCLUDE_DIR)
    include_directories(${JSON_INCLUDE_DIR})
else()
    message(WARNING "nlohmann/json.hpp not found, using fallback include location")
    include_directories("/usr/include/nlohmann")
endif()

# Find dl library (needed for dynamic loading in pluginmanager)
find_library(DL_LIBRARY dl)
if(NOT DL_LIBRARY)
    message(FATAL_ERROR "dl library not found")
endif()

# Option for static linking (better portability across Linux distributions)
option(STATIC_LINKING "Link libraries statically for better portability" OFF)

# Modified target linking approach that works with older CMake
# Create a helper function to handle CURL linking safely
function(link_curl_to_target target_name)
    if(TARGET CURL::libcurl)
        target_link_libraries(${target_name} CURL::libcurl)
    else()
        target_link_libraries(${target_name} ${CURL_LIBRARIES})
    endif()
endfunction()

# Modify the executable target to create platform-specific builds
if(APPLE)
    # macOS build
    add_executable(DevToolsTerminal_macOS ${SOURCE_FILES})
    set_target_properties(DevToolsTerminal_macOS PROPERTIES
        OUTPUT_NAME "DevToolsTerminal"
        RUNTIME_OUTPUT_DIRECTORY ${MACOS_OUTPUT_DIR}
    )
    link_curl_to_target(DevToolsTerminal_macOS)
    target_link_libraries(DevToolsTerminal_macOS Threads::Threads)
endif()

# Linux build
add_executable(DevToolsTerminal_Linux ${SOURCE_FILES})
set_target_properties(DevToolsTerminal_Linux PROPERTIES
    OUTPUT_NAME "DevToolsTerminal"
    RUNTIME_OUTPUT_DIRECTORY ${LINUX_OUTPUT_DIR}
)
link_curl_to_target(DevToolsTerminal_Linux)
target_link_libraries(DevToolsTerminal_Linux 
    Threads::Threads
    ${DL_LIBRARY}
)

# Set rpath for Linux builds
if(UNIX AND NOT APPLE)
    set_target_properties(DevToolsTerminal_Linux PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# If static linking is enabled, try to find static versions of libraries
if(STATIC_LINKING)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .so)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Package configuration
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Developer Tools Terminal")
set(CPACK_PACKAGE_VENDOR "DevToolsTerminal")

# Include CPack to enable packaging
include(CPack)

# Update install locations
install(TARGETS DevToolsTerminal_Linux
    RUNTIME DESTINATION bin/linux
    CONFIGURATIONS Release
)

if(APPLE)
    install(TARGETS DevToolsTerminal_macOS
        RUNTIME DESTINATION bin/macos
        CONFIGURATIONS Release
    )
endif()
