cmake_minimum_required(VERSION 3.10)

# Set the project name
project(DevToolsTerminal)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17) # Ensure C++17 standard is used
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include the header files directory
include_directories(include)

find_package(CURL REQUIRED)

# Add the include directories for CURL
include_directories(${CURL_INCLUDE_DIRS})

# Add the executable
add_executable(DevToolsTerminal-LITE
    src/main.cpp
    include/openaipromptengine.h
    # ...other source files...
)

# Link the CURL library
target_link_libraries(DevToolsTerminal-LITE ${CURL_LIBRARIES})
