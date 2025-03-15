cmake_minimum_required(VERSION 3.10)
project(GUIPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find Qt packages
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# Include project headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

# Add source files
set(SOURCES
    GUIPlugin.cpp
)

# Add header files
set(HEADERS
    GUIPlugin.h
)

# Create shared library
add_library(GUIPlugin SHARED ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(GUIPlugin Qt5::Core Qt5::Widgets)

# Set output directory
set_target_properties(GUIPlugin
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)

# Set macOS specific settings
if(APPLE)
    set_target_properties(GUIPlugin
        PROPERTIES
        SUFFIX ".dylib"
    )
endif()
