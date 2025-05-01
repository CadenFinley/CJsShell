cmake_minimum_required(VERSION 3.10)

# Project name and version
project(cjsh VERSION 0.1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compiler warnings
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

if(APPLE)
  # Detect if running on Apple Silicon
  execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(ARCH STREQUAL "arm64")
    message(STATUS "Detected Apple Silicon (arm64)")
    
    # Set the architecture explicitly at the CMake level
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
    
    # Remove all CPU-specific flags to prevent targeting issues
    string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    # Also remove -march flags that might cause issues
    string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    # Set basic architecture flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch arm64")
    
    # Only use minimal architecture specification
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
  endif()
endif()

# Make sure to set these before adding subdirectories
if(ARCH STREQUAL "arm64")
  set(IC_APPLE_SILICON ON CACHE BOOL "Targeting Apple Silicon" FORCE)
else()
  set(IC_APPLE_SILICON OFF CACHE BOOL "Targeting Apple Silicon" FORCE)
endif()

# Find and include curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Find threads package
find_package(Threads REQUIRED)

# Find dlopen library for dynamic loading
if(UNIX AND NOT APPLE)
  # On Linux, explicitly link with libdl
  find_library(DL_LIBRARY dl REQUIRED)
endif()

# nlohmann/json - header only library
# Check both locations where json.hpp might be found
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include/nlohmann/json.hpp")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include)
  message(STATUS "Found nlohmann/json in third_party/json/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann/json.hpp")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)
  message(STATUS "Found nlohmann/json in external/nlohmann")
else()
  message(WARNING "nlohmann/json.hpp not found in expected locations. Downloading single header...")
  
  # Create directories to store the header
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann")
  
  # Download the single header file directly
  file(DOWNLOAD
    "https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann/json.hpp"
    SHOW_PROGRESS
    TLS_VERIFY ON
  )
  
  # Add the directory to include paths
  include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
  message(STATUS "Downloaded nlohmann/json.hpp to ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann")
endif()

# isocline library (part of codebase)
# Pass specific options to isocline to prevent CPU targeting
set(ISOCLINE_DISABLE_CPU_TARGETING ON CACHE BOOL "Disable CPU targeting in isocline" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/isocline)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vendor/isocline/include)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# build mvdan/sh bridge as a static C library
add_custom_command(
  OUTPUT 
    ${CMAKE_CURRENT_BINARY_DIR}/libmvdan_sh.a 
    ${CMAKE_CURRENT_BINARY_DIR}/libmvdan_sh.h
  COMMAND go mod tidy
  # generate into build folder
  COMMAND go build -o ${CMAKE_CURRENT_BINARY_DIR}/libmvdan_sh.a -buildmode=c-archive bridge.go
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mvdan_sh
  VERBATIM
)
add_custom_target(mvdan_sh_lib ALL
  DEPENDS 
    ${CMAKE_CURRENT_BINARY_DIR}/libmvdan_sh.a 
    ${CMAKE_CURRENT_BINARY_DIR}/libmvdan_sh.h
)

# define and link your main executable, including the Go bridge
add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} mvdan_sh_lib)
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}   # for libmvdan_sh.h
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/libmvdan_sh.a
  ${CURL_LIBRARIES}
  isocline
  Threads::Threads
)
if(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${DL_LIBRARY})
endif()

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Testing (optional)
option(BUILD_TESTS "Build the tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
