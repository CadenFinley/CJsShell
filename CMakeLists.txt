cmake_minimum_required(VERSION 3.10)

# Check for Unix platform - fail on Windows
if(WIN32)
    message(FATAL_ERROR "DevToolsTerminal is currently only supported on Unix-based systems (Linux/macOS)")
endif()

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

# Link the libraries
target_link_libraries(DevToolsTerminal 
    ${CURL_LIBRARIES}
    Threads::Threads
    ${DL_LIBRARY}
)

# Enable testing with CTest
enable_testing()

# Add the tests subdirectory
add_subdirectory(tests)

# Set install locations
install(TARGETS DevToolsTerminal DESTINATION bin)
