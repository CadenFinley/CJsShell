cmake_minimum_required(VERSION 3.10)

project(cjsh VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

if(APPLE)
  execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(ARCH STREQUAL "arm64")
    message(STATUS "Detected Apple Silicon (arm64)")
    
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
    
    string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch arm64")
    
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
  endif()
endif()

if(ARCH STREQUAL "arm64")
  set(IC_APPLE_SILICON ON CACHE BOOL "Targeting Apple Silicon" FORCE)
else()
  set(IC_APPLE_SILICON OFF CACHE BOOL "Targeting Apple Silicon" FORCE)
endif()

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

find_package(Threads REQUIRED)

if(UNIX AND NOT APPLE)
  find_library(DL_LIBRARY dl REQUIRED)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include/nlohmann/json.hpp")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include)
  message(STATUS "Found nlohmann/json in third_party/json/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann/json.hpp")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)
  message(STATUS "Found nlohmann/json in external/nlohmann")
else()
  message(WARNING "nlohmann/json.hpp not found in expected locations. Downloading single header...")
  
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann")
  
  file(DOWNLOAD
    "https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann/json.hpp"
    SHOW_PROGRESS
    TLS_VERIFY ON
  )
  
  include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
  message(STATUS "Downloaded nlohmann/json.hpp to ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann")
endif()

set(ISOCLINE_DISABLE_CPU_TARGETING ON CACHE BOOL "Disable CPU targeting in isocline" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/isocline)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vendor/isocline/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  ${CURL_LIBRARIES}
  isocline
  Threads::Threads
)
if(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${DL_LIBRARY})
endif()

add_custom_target(format
  COMMAND clang-format -i -style=file
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/include/*.h
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

option(BUILD_TESTS "Build the tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
