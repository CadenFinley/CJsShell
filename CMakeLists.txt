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
    # On Linux, try to find json library (using pkg-config or direct include)
    include_directories("/usr/include/nlohmann")
endif()

# Link the CURL library
target_link_libraries(DevToolsTerminal 
    ${CURL_LIBRARIES}
    Threads::Threads  # Add this to link against pthread
)

# Set install locations
install(TARGETS DevToolsTerminal DESTINATION bin)
