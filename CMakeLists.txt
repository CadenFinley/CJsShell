cmake_minimum_required(VERSION 3.10)

project(cjsh VERSION 2.1.13 LANGUAGES C CXX)

# Add option for forcing 32-bit build
option(FORCE_32BIT "Force 32-bit build" OFF)

# Detect architecture using shell script
if(FORCE_32BIT)
  execute_process(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/detect_arch.sh true
    OUTPUT_VARIABLE ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE ARCH_DETECTION_RESULT
  )
else()
  execute_process(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmake/detect_arch.sh
    OUTPUT_VARIABLE ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE ARCH_DETECTION_RESULT
  )
endif()

# Check if architecture detection was successful
if(ARCH_DETECTION_RESULT EQUAL 0)
  set(ARCH "${ARCH}" CACHE INTERNAL "Architecture" FORCE)
  message(STATUS "Detected architecture: ${ARCH}")
else()
  # Fallback to x86_64 if script fails
  set(ARCH "x86_64" CACHE INTERNAL "Architecture" FORCE)
  message(WARNING "Architecture detection script failed, defaulting to x86_64")
endif()

# Include common settings
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.common.cmake)

# Include architecture-specific settings
if(ARCH STREQUAL "x86")
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.x86.cmake)
else()
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.${ARCH}.cmake)
endif()


# File globbing and main target definition
file(GLOB_RECURSE SOURCES 
  "src/*.cpp" 
  "src/*.c" 
  "src/builtin/*.cpp" 
  "src/builtin/*.c" 
  "src/builtins/*.cpp" 
  "src/builtins/*.c" 
  "src/utils/*.cpp" 
  "src/utils/*.c" 
  "src/assistant/*.cpp" 
  "src/isocline/*.cpp"
)

# Explicitly list isocline .c files that should be compiled
file(GLOB ISOCLINE_MAIN_C 
  "src/isocline/attr.c" 
  "src/isocline/bbcode.c" 
  "src/isocline/bbcode_colors.c" 
  "src/isocline/common.c" 
  "src/isocline/completers.c" 
  "src/isocline/completions.c" 
  "src/isocline/editline.c" 
  "src/isocline/highlight.c" 
  "src/isocline/history.c" 
  "src/isocline/isocline.c" 
  "src/isocline/stringbuf.c" 
  "src/isocline/term.c" 
  "src/isocline/tty.c" 
  "src/isocline/tty_esc.c" 
  "src/isocline/undo.c" 
  "src/isocline/wcwidth.c"
)

# Add our shim file first in the sources list
set(SHIM_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/libintl_shim.cpp"
)

# Remove files that are included by other files and shouldn't be compiled separately
list(APPEND SOURCES ${ISOCLINE_MAIN_C} ${SHIM_FILES})

# Find files in sources that should be excluded
file(GLOB EXCLUDE_FILES
  "src/isocline/editline_completion.c"
  "src/isocline/editline_help.c"
  "src/isocline/editline_history.c"
  "src/isocline/term_color.c"
)

# Remove the excluded files from the sources list
list(REMOVE_ITEM SOURCES ${EXCLUDE_FILES})


# Set -Os for the default binary
add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_options(${PROJECT_NAME} PRIVATE -Os)

# Set isocline library to use separate objects
add_definitions(-DIC_SEPARATE_OBJS=1)

# Create file-specific compile flags for isocline files to disable warnings as errors
set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/bbcode_colors.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/completers.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/editline.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/term.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/term_color.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/tty.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/tty_esc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/wcwidth.c
  PROPERTIES COMPILE_FLAGS "-Wno-error"
)

# Find nlohmann JSON library
find_package(nlohmann_json QUIET)

if(NOT nlohmann_json_FOUND)
  # Fallback: Use the single header file if package not found
  message(STATUS "nlohmann_json package not found, using single header file approach")
  
  # Check if the external/nlohmann/json.hpp file exists
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann/json.hpp")
    message(STATUS "Using json.hpp from external/nlohmann directory")
    add_library(nlohmann_json INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    target_include_directories(nlohmann_json INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external")
  else()
    message(FATAL_ERROR "nlohmann_json header not found. Please run 'mkdir -p external/nlohmann && wget -O external/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp'")
  endif()
endif()

# Add required include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/isocline
    ${CMAKE_CURRENT_SOURCE_DIR}/include/builtin
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/include/prompt
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
)

# Link against libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    curl
    pthread
    dl
    nlohmann_json::nlohmann_json
)

# Prevent JSON library from using certain features
add_definitions(-DJSON_NOEXCEPTION=1)
add_definitions(-DJSON_USE_IMPLICIT_CONVERSIONS=1)

add_custom_target(format
  COMMAND clang-format -i -style=file
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/include/*.h
)

# Add option for building multiple binaries with different optimizations
option(BUILD_MULTIPLE_BINARIES "Build multiple binaries with different optimizations" OFF)

if(BUILD_MULTIPLE_BINARIES)
  # Speed-optimized binary 03
  add_executable(${PROJECT_NAME}_speed03 ${SOURCES})
  target_compile_options(${PROJECT_NAME}_speed03 PRIVATE -O3)
  target_link_libraries(${PROJECT_NAME}_speed03 PRIVATE
      curl
      pthread
      dl
      nlohmann_json::nlohmann_json
  )

    # Speed-optimized binary 02
  add_executable(${PROJECT_NAME}_speed02 ${SOURCES})
  target_compile_options(${PROJECT_NAME}_speed02 PRIVATE -O2)
  target_link_libraries(${PROJECT_NAME}_speed02 PRIVATE
      curl
      pthread
      dl
      nlohmann_json::nlohmann_json
  )
endif()
