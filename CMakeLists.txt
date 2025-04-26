cmake_minimum_required(VERSION 3.10)

# Project name and version
project(CJsShell VERSION 0.1.0 LANGUAGES CXX)

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

# Find and include curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# nlohmann/json - header only library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include)
# If json is not available in the path above, you may need to fetch it:
# include(FetchContent)
# FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
# FetchContent_MakeAvailable(json)

# isocline library (part of codebase)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/isocline)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/isocline/include)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Define the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES} isocline)

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Testing (optional)
option(BUILD_TESTS "Build the tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
