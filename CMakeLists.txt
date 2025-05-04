cmake_minimum_required(VERSION 3.10)

project(cjsh VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

# Try to find CURL, but don't require it
find_package(CURL QUIET)

# If CURL was not found, download and build it
if(NOT CURL_FOUND)
  message(STATUS "CURL not found. Attempting to download and build it...")
  
  # Check if FetchContent is available (CMake 3.11+)
  if(CMAKE_VERSION VERSION_LESS 3.11)
    message(FATAL_ERROR "CURL not found and CMake version < 3.11. Please install libcurl development files.")
  endif()
  
  include(FetchContent)
  
  # Try to find OpenSSL first
  find_package(OpenSSL QUIET)
  if(NOT OpenSSL_FOUND)
    # Try common OpenSSL locations
    if(APPLE)
      # Homebrew / MacPorts common paths
      set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" CACHE PATH "OpenSSL root directory")
      if(NOT EXISTS "${OPENSSL_ROOT_DIR}")
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl" CACHE PATH "OpenSSL root directory" FORCE)
      endif()
    endif()
    # Try again with our suggestions
    find_package(OpenSSL QUIET)
  endif()
  
  # SSL build option - default to ON if OpenSSL found, OFF otherwise
  option(CURL_USE_SSL "Build CURL with SSL support" ${OpenSSL_FOUND})
  
  # Force specific configurations before fetching curl
  set(CURL_DISABLE_LDAP ON CACHE BOOL "Disable LDAP" FORCE)
  set(CURL_DISABLE_LDAPS ON CACHE BOOL "Disable LDAPS" FORCE)
  set(ENABLE_IPV6 ON CACHE BOOL "Enable IPv6" FORCE)
  
  # Enable curl config process to detect system headers
  set(CMAKE_USE_SYSTEM_LIBRARY_IN_CURL ON CACHE BOOL "Use system library in curl" FORCE)
  set(CURL_ENABLE_CURL_CONFIG ON CACHE BOOL "Enable curl_config.h" FORCE)
  
  # Add compiler flags to handle gethostname
  add_compile_options(-D_GNU_SOURCE)
  add_compile_definitions(HAVE_GETHOSTNAME)
  
  FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_2_1 # Use a specific stable version
  )
  
  # Use the recommended MakeAvailable instead of Populate
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.14)
    # Set variables before fetching
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    
    # Add system header definitions to fix gethostname() error
    set(HAVE_UNISTD_H 1 CACHE INTERNAL "Have unistd.h header" FORCE)
    set(HAVE_SYS_SOCKET_H 1 CACHE INTERNAL "Have sys/socket.h header" FORCE)
    set(HAVE_NETDB_H 1 CACHE INTERNAL "Have netdb.h header" FORCE)
    
    # Set SSL options
    if(NOT CURL_USE_SSL)
      set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
      set(HTTP_ONLY ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_CRYPTO_AUTH ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_DICT ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_FILE ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_FTP ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_GOPHER ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_IMAP ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_POP3 ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_RTMP ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_RTSP ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_SMB ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_SMTP ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_TELNET ON CACHE BOOL "" FORCE)
      set(CURL_DISABLE_TFTP ON CACHE BOOL "" FORCE)
    endif()
    
    message(STATUS "Downloading and configuring CURL...")
    FetchContent_MakeAvailable(curl)
    set(CURL_INCLUDE_DIRS ${curl_SOURCE_DIR}/include)
    set(CURL_LIBRARIES libcurl)
  else()
    # Fallback for older CMake versions
    FetchContent_GetProperties(curl)
    if(NOT curl_POPULATED)
      message(STATUS "Downloading CURL...")
      FetchContent_Populate(curl)
      
      # Disable unnecessary features to speed up build
      set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
      set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
      set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
      
      # Add system header definitions to fix gethostname() error
      set(HAVE_UNISTD_H 1 CACHE INTERNAL "Have unistd.h header" FORCE)
      set(HAVE_SYS_SOCKET_H 1 CACHE INTERNAL "Have sys/socket.h header" FORCE)
      set(HAVE_NETDB_H 1 CACHE INTERNAL "Have netdb.h header" FORCE)
      
      # Set SSL options
      if(NOT CURL_USE_SSL)
        set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
        set(HTTP_ONLY ON CACHE BOOL "" FORCE)
        set(CURL_DISABLE_CRYPTO_AUTH ON CACHE BOOL "" FORCE)
      endif()
      
      message(STATUS "Configuring CURL...")
      add_subdirectory(${curl_SOURCE_DIR} ${curl_BINARY_DIR} EXCLUDE_FROM_ALL)
      
      set(CURL_INCLUDE_DIRS ${curl_SOURCE_DIR}/include)
      set(CURL_LIBRARIES libcurl)
    endif()
  endif()
endif()

include_directories(${CURL_INCLUDE_DIRS})

find_package(Threads REQUIRED)

if(UNIX AND NOT APPLE)
  find_library(DL_LIBRARY dl REQUIRED)
endif()

# Find GLib which provides glib.h
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(GLIB QUIET glib-2.0)
  if(GLIB_FOUND)
    include_directories(${GLIB_INCLUDE_DIRS})
    message(STATUS "Found GLib via pkg-config: ${GLIB_VERSION}")
  else()
    message(WARNING "GLib not found via pkg-config, trying alternate methods...")
    # Try to find GLib in common locations
    find_path(GLIB_INCLUDE_DIRS glib.h
      PATH_SUFFIXES glib-2.0
      PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        /opt/homebrew/include
        $ENV{GLIB_DIR}/include
    )
    
    if(GLIB_INCLUDE_DIRS)
      # Also need to find glibconfig.h which is in a different location
      find_path(GLIB_CONFIG_INCLUDE_DIR glibconfig.h
        PATH_SUFFIXES 
          glib-2.0/include
          lib/glib-2.0/include
          include/glib-2.0/include
        PATHS
          /usr/lib
          /usr/local/lib
          /opt/local/lib
          /opt/homebrew/lib
          /usr/include
          /usr/local/include
          $ENV{GLIB_DIR}/lib
      )
      
      if(GLIB_CONFIG_INCLUDE_DIR)
        set(GLIB_INCLUDE_DIRS ${GLIB_INCLUDE_DIRS} ${GLIB_CONFIG_INCLUDE_DIR})
        find_library(GLIB_LIBRARIES NAMES glib-2.0 glib
          PATHS
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /opt/homebrew/lib
            $ENV{GLIB_DIR}/lib
        )
        
        if(GLIB_LIBRARIES)
          message(STATUS "Found GLib manually: ${GLIB_LIBRARIES}")
          include_directories(${GLIB_INCLUDE_DIRS})
        else()
          message(FATAL_ERROR "GLib library not found. Please install GLib development package.")
        endif()
      else()
        message(FATAL_ERROR "glibconfig.h not found. Please install GLib development package.")
      endif()
    else()
      message(FATAL_ERROR "glib.h not found. Please install GLib development package.")
    endif()
  endif()
else()
  message(FATAL_ERROR "pkg-config not found. Unable to locate GLib. Please install pkg-config and GLib development package.")
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

# Add warning flags only to our project target, not to libraries
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  ${CURL_LIBRARIES}
  isocline
  Threads::Threads
  ${GLIB_LIBRARIES}
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
