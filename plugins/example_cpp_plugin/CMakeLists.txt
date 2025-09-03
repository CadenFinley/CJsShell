cmake_minimum_required(VERSION 3.10)
project(example_cpp_plugin)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Determine the output file extension based on the platform
if(APPLE)
    set(PLUGIN_EXTENSION ".dylib")
else()
    set(PLUGIN_EXTENSION ".so")
endif()

# Create the shared library
add_library(example_cpp_plugin SHARED
    all_features_plugin.cpp
)

# Set output name
set_target_properties(example_cpp_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "example_cpp_plugin"
    SUFFIX ${PLUGIN_EXTENSION}
)

# Link against necessary libraries
target_link_libraries(example_cpp_plugin PRIVATE
    Threads::Threads
)

# Set additional link flags for dynamic resolution of symbols on macOS
if(APPLE)
    target_link_options(example_cpp_plugin PRIVATE "-undefined" "dynamic_lookup")
endif()

# Installation rules
if(APPLE)
    install(TARGETS example_cpp_plugin
            LIBRARY DESTINATION ~/.config/cjsh/plugins)
else()
    install(TARGETS example_cpp_plugin
            LIBRARY DESTINATION ~/.config/cjsh/plugins)
endif()
