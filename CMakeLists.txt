cmake_minimum_required(VERSION 3.10)

project(cjsh VERSION 2.1.13 LANGUAGES C CXX)

# Add option for forcing 32-bit build
option(FORCE_32BIT "Force 32-bit build" OFF)

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
elseif(FORCE_32BIT OR CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(ARCH "x86" CACHE INTERNAL "Architecture" FORCE)
  message(STATUS "Configuring for x86 (32-bit) architecture")
else()
  set(ARCH "x86_64" CACHE INTERNAL "Architecture" FORCE)
  message(STATUS "Assuming x86_64 architecture")
endif()

# Include common settings
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.common.cmake)

# Include architecture-specific settings
if(FORCE_32BIT OR CMAKE_SIZEOF_VOID_P EQUAL 4)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.x86.cmake)
else()
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeLists.${ARCH}.cmake)
endif()

# Removed unnecessary pkg-config dependency
# find_package(PkgConfig REQUIRED)

# File globbing and main target definition
file(GLOB_RECURSE SOURCES 
  "src/*.cpp" 
  "src/*.c" 
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

add_executable(${PROJECT_NAME} ${SOURCES})

# Find additional libraries needed for libcurl
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Optional protocol libraries removed for simpler build
# find_package(LDAP)
# find_package(Libssh2)
# find_package(LibRTMP)
# find_package(LibPSL)
# find_package(LibIDN2)
# find_package(Libnghttp2)

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
find_package(nlohmann_json REQUIRED)

# Add required include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

# Link against libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    curl
    pthread
    dl
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Use dynamic linking for libcurl instead of static
add_definitions(-DCURL_STATICLIB=0)

# Prevent JSON library from using certain features
add_definitions(-DJSON_NOEXCEPTION=1)
add_definitions(-DJSON_USE_IMPLICIT_CONVERSIONS=1)

add_custom_target(format
  COMMAND clang-format -i -style=file
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/include/*.h
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

option(BUILD_TESTS "Build the tests" OFF)
if(BUILD_TESTS)
  # Fetch GoogleTest
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
  )
  FetchContent_MakeAvailable(googletest)

  enable_testing()

  # Collect parser sources
  set(PARSER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp
  )

  # Define parser_tests executable
  add_executable(parser_tests
    tests/parser_tests.cpp
    ${PARSER_SRCS}
  )

  target_include_directories(parser_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(parser_tests PRIVATE
    gtest_main
    pthread
  )

  add_test(NAME ParserTests COMMAND parser_tests --gtest_color=yes)
  
  # Optional: To see more detailed output, uncomment one of these:
  # set_tests_properties(ParserTests PROPERTIES ENVIRONMENT "GTEST_OUTPUT=xml:${CMAKE_BINARY_DIR}/test_results/")
  # set_tests_properties(ParserTests PROPERTIES ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1")
endif()
