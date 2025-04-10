cmake_minimum_required(VERSION 3.10)

# Set the project name
project(DevToolsTerminal)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17) # Ensure C++17 standard is used
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include the header files directory
include_directories(src)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)  # Add this to find pthread library

# Source files
set(SOURCE_FILES
    src/main.cpp
    src/terminalpassthrough.cpp
    src/openaipromptengine.cpp
    src/pluginmanager.cpp
    src/thememanager.cpp
)

# Add the include directories for CURL
include_directories(${CURL_INCLUDE_DIRS})

# Add the executable
add_executable(DevToolsTerminal ${SOURCE_FILES})

# Find nlohmann_json package (different methods for different platforms)
if(WIN32)
    # On Windows, nlohmann_json is handled by vcpkg
else()
    # On Linux, try multiple common locations for the nlohmann/json.hpp file
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
endif()

# Find dl library on Unix-like systems (needed for dynamic loading in pluginmanager)
if(UNIX)
    find_library(DL_LIBRARY dl)
    if(NOT DL_LIBRARY)
        message(FATAL_ERROR "dl library not found")
    endif()
endif()

# Link the libraries
target_link_libraries(DevToolsTerminal 
    ${CURL_LIBRARIES}
    Threads::Threads  # Add this to link against pthread
    $<$<PLATFORM_ID:Linux,Darwin>:${DL_LIBRARY}>  # Conditionally link dl library on Linux/macOS
)

# Set install locations
install(TARGETS DevToolsTerminal DESTINATION bin)
