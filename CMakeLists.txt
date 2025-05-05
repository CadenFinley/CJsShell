cmake_minimum_required(VERSION 3.10)

project(cjsh VERSION 0.1.0 LANGUAGES C CXX)

# Detect architecture
if(APPLE)
  execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(ARCH STREQUAL "arm64")
    set(ARCH "arm64" CACHE INTERNAL "Architecture" FORCE)
    message(STATUS "Detected Apple Silicon (arm64)")
  else()
    set(ARCH "x86_64" CACHE INTERNAL "Architecture" FORCE)
    message(STATUS "Detected macOS x86_64")
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
  set(ARCH "arm64" CACHE INTERNAL "Architecture" FORCE)
  message(STATUS "Detected ARM64 architecture")
else()
  if(NOT DEFINED ARCH)
    set(ARCH "x86_64" CACHE INTERNAL "Architecture" FORCE)
    message(STATUS "Assuming x86_64 architecture")
  endif()
endif()

# Include common settings
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.common.cmake)

# Include architecture-specific settings
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.${ARCH}.cmake)

# File globbing and main target definition
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
add_executable(${PROJECT_NAME} ${SOURCES})

# Final linking and installation
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE
  ${CURL_LIBRARIES}
  isocline
  Threads::Threads
  ${GLIB_LIBRARIES}
)

# Platform-specific linking
if(UNIX)
  if(NOT APPLE)
    # Static linking flags only for non-Apple Unix (Linux, etc.)
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
    if(DL_LIBRARY)
      target_link_libraries(${PROJECT_NAME} PRIVATE ${DL_LIBRARY})
    endif()
  endif()
endif()

# Ensure standard headers needed by json library
target_compile_definitions(${PROJECT_NAME} PRIVATE 
  HAVE_ARRAY=1
  HAVE_CSTDDEF=1
  HAVE_UTILITY=1
)

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
