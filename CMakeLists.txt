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

# File globbing and main target definition
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
add_executable(${PROJECT_NAME} ${SOURCES})

# Configure curl build options before adding the subdirectory
set(BUILD_CURL_EXE OFF CACHE BOOL "Don't build the curl executable" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(CURL_STATIC_CRT ON CACHE BOOL "Use static CRT" FORCE)

# Disable all optional protocols we don't need
set(CURL_DISABLE_LDAP ON CACHE BOOL "Disable LDAP support" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "Disable LDAPS support" FORCE)
set(CURL_DISABLE_RTSP ON CACHE BOOL "Disable RTSP support" FORCE)
set(CURL_DISABLE_DICT ON CACHE BOOL "Disable DICT support" FORCE)
set(CURL_DISABLE_FILE ON CACHE BOOL "Disable FILE support" FORCE)
set(CURL_DISABLE_TELNET ON CACHE BOOL "Disable TELNET support" FORCE)
set(CURL_DISABLE_TFTP ON CACHE BOOL "Disable TFTP support" FORCE)
set(CURL_DISABLE_LIBCURL_OPTION ON CACHE BOOL "Disable install of curl-config" FORCE)

# Completely disable libpsl dependency
set(CURL_USE_LIBPSL OFF CACHE BOOL "Disable PSL support" FORCE)
set(CURL_DISABLE_PSL ON CACHE BOOL "Disable PSL support" FORCE)

# Add vendor subdirectories
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl)

# Check if isocline target exists before adding its subdirectory
if(NOT TARGET isocline)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/isocline 
                   ${CMAKE_CURRENT_BINARY_DIR}/isocline_build)
endif()

# We still need these for curl dependencies
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Add required include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/isocline/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl/include
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

# Link against libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    libcurl  # Changed to the actual target name from vendor/curl
    isocline # Link against the local isocline library
    pthread
    dl
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

# Ensure standard headers needed by json library
target_compile_definitions(${PROJECT_NAME} PRIVATE 
  HAVE_ARRAY=1
  HAVE_CSTDDEF=1
  HAVE_UTILITY=1
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
