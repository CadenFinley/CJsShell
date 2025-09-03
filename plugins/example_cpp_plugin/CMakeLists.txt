cmake_minimum_required(VERSION 3.10)
project(all_features_plugin)

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
add_library(all_features_plugin SHARED
    all_features_plugin.cpp
)

# Set output name
set_target_properties(all_features_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "all_features_plugin"
    SUFFIX ${PLUGIN_EXTENSION}
)

# Link against necessary libraries
target_link_libraries(all_features_plugin PRIVATE
    Threads::Threads
)

# Set additional link flags for dynamic resolution of symbols on macOS
if(APPLE)
    target_link_options(all_features_plugin PRIVATE "-undefined" "dynamic_lookup")
endif()

# Installation rules
if(APPLE)
    install(TARGETS all_features_plugin
            LIBRARY DESTINATION ~/.config/cjsh/plugins)
else()
    install(TARGETS all_features_plugin
            LIBRARY DESTINATION ~/.config/cjsh/plugins)
endif()
