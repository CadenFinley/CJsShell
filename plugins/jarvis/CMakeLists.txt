cmake_minimum_required(VERSION 3.10)
project(jarvis LANGUAGES CXX)

# Use modern C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include_directories(${CMAKE_SOURCE_DIR}/../../include)

# Plugin source
set(SOURCES
    jarvis.cpp
)

# Build shared library (plugin)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Platform-specific settings
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".dylib"
        PREFIX ""                # no "lib" prefix (CJSH plugin expects raw name)
        LINK_FLAGS "-undefined dynamic_lookup -flat_namespace"
    )
elseif(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".so"
        PREFIX ""                # same on Linux
    )
endif()