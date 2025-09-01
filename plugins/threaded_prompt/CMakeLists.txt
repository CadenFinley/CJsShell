cmake_minimum_required(VERSION 3.10)
project(threaded_prompt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add threading support
find_package(Threads REQUIRED)

# Get the absolute path to the main project include directory
get_filename_component(MAIN_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)

# Include directories - using absolute paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${MAIN_PROJECT_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Define the source files
set(SOURCES
    threaded_prompt.cpp
)

# Create a shared library
add_library(threaded_prompt SHARED ${SOURCES})

# Link with the thread library
target_link_libraries(threaded_prompt PRIVATE Threads::Threads)

# Set output directory and proper extension/flags
if(APPLE)
    set_target_properties(threaded_prompt PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        SUFFIX ".dylib"
        PREFIX ""
        # Permit undefined symbols (from host application) during linking
        LINK_FLAGS "-undefined dynamic_lookup -flat_namespace"
    )
else()
    set_target_properties(threaded_prompt PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        SUFFIX ".so"
        PREFIX ""
    )
endif()

# Installation
install(TARGETS threaded_prompt
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)
