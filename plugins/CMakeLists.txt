cmake_minimum_required(VERSION 3.10)
project(cjsh_plugins VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directory for plugins
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

# Create shared library for hello_plugin
add_library(hello_plugin SHARED hello_plugin.cpp)

# Rename output with appropriate file extension based on platform
if(APPLE)
    set_target_properties(hello_plugin PROPERTIES PREFIX "" SUFFIX ".dylib")
elseif(UNIX)
    set_target_properties(hello_plugin PROPERTIES PREFIX "" SUFFIX ".so")
elseif(WIN32)
    set_target_properties(hello_plugin PROPERTIES PREFIX "" SUFFIX ".dll")
endif()

# Include directories
target_include_directories(hello_plugin PRIVATE ${CMAKE_SOURCE_DIR}/../include)

# Optionally link against any required libraries
# target_link_libraries(hello_plugin PRIVATE some_dependency)

# Add installation rules
install(TARGETS hello_plugin
        LIBRARY DESTINATION lib/cjsh/plugins)
