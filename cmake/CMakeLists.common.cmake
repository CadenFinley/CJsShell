# Common CMake configuration for all architectures

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler flags
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Common warning flags
  add_compile_options(-Wall -Wextra -Wpedantic)
  
  # Debug flags
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
  endif()
  
  # Release flags
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
  endif()
endif()

# Check for 32-bit build
if(CMAKE_SIZEOF_VOID_P EQUAL 4 OR FORCE_32BIT)
  set(BUILD_32BIT TRUE)
  message(STATUS "Configuring 32-bit build")
  
  # On Linux, make sure we have the right libraries
  if(UNIX AND NOT APPLE)
    # Explicitly check for 32-bit libraries
    include(CheckLibraryExists)
    CHECK_LIBRARY_EXISTS(gcc_s __register_frame_info /usr/lib32 HAVE_LIB32GCC_S)
    
    if(NOT HAVE_LIB32GCC_S)
      message(WARNING "32-bit libgcc not found in /usr/lib32. Build may fail.")
      message(STATUS "Run tool-scripts/linux_build_helper.sh install-32bit to install required packages")
    endif()
    
    # Set 32-bit specific flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    
    # Add 32-bit library paths
    link_directories(/usr/lib32)
  endif()
else()
  set(BUILD_32BIT FALSE)
  message(STATUS "Configuring 64-bit build")
endif()

# Common include paths
include_directories(${CMAKE_SOURCE_DIR}/include)

# Common C++ settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Prefer static linking when possible
set(BUILD_SHARED_LIBS OFF)

# Configure static linking - handle platform differences
if(UNIX)
  if(NOT APPLE)
    # Static linking flags only for non-Apple Unix (Linux, etc.)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  else()
    # macOS-specific linking options (Apple's Clang doesn't support -static-libgcc)
    message(STATUS "Using macOS-specific linking options")
  endif()
endif()

# Common warning flags
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  
  # Add -Werror for all files except isocline
  # This approach still allows us to catch warnings in our own code
  add_compile_options($<$<NOT:$<STREQUAL:$<TARGET_PROPERTY:SOURCE_DIR>,${CMAKE_CURRENT_SOURCE_DIR}/src/isocline>>:-Werror>)
endif()

# Common dependencies
find_package(Threads REQUIRED)

if(UNIX AND NOT APPLE)
  find_library(DL_LIBRARY dl REQUIRED)
endif()

# Try to find CURL
find_package(CURL QUIET)

# If CURL was not found, download and build it
if(NOT CURL_FOUND)
  message(STATUS "CURL not found. Attempting to download and build it...")
  
  if(CMAKE_VERSION VERSION_LESS 3.11)
    message(FATAL_ERROR "CURL not found and CMake version < 3.11. Please install libcurl development files.")
  endif()
  
  include(FetchContent)
  
  find_package(OpenSSL QUIET)
  if(NOT OpenSSL_FOUND)
    if(APPLE)
      set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" CACHE PATH "OpenSSL root directory")
      if(NOT EXISTS "${OPENSSL_ROOT_DIR}")
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl" CACHE PATH "OpenSSL root directory" FORCE)
      endif()
    endif()
    find_package(OpenSSL QUIET)
  endif()
  
  option(CURL_USE_SSL "Build CURL with SSL support" ${OpenSSL_FOUND})
  
  set(CURL_DISABLE_LDAP ON CACHE BOOL "Disable LDAP" FORCE)
  set(CURL_DISABLE_LDAPS ON CACHE BOOL "Disable LDAPS" FORCE)
  set(ENABLE_IPV6 ON CACHE BOOL "Enable IPv6" FORCE)
  set(CMAKE_USE_SYSTEM_LIBRARY_IN_CURL ON CACHE BOOL "Use system library in curl" FORCE)
  set(CURL_ENABLE_CURL_CONFIG ON CACHE BOOL "Enable curl_config.h" FORCE)
  
  add_compile_options(-D_GNU_SOURCE)
  add_compile_definitions(HAVE_GETHOSTNAME)
  
  FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_2_1
  )
  
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.14)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    
    set(HAVE_UNISTD_H 1 CACHE INTERNAL "Have unistd.h header" FORCE)
    set(HAVE_SYS_SOCKET_H 1 CACHE INTERNAL "Have sys/socket.h header" FORCE)
    set(HAVE_NETDB_H 1 CACHE INTERNAL "Have netdb.h header" FORCE)
    
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
    
    FetchContent_MakeAvailable(curl)
    set(CURL_INCLUDE_DIRS ${curl_SOURCE_DIR}/include)
    set(CURL_LIBRARIES libcurl)
  else()
    FetchContent_GetProperties(curl)
    if(NOT curl_POPULATED)
      FetchContent_Populate(curl)
      
      set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
      set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
      set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
      
      set(HAVE_UNISTD_H 1 CACHE INTERNAL "Have unistd.h header" FORCE)
      set(HAVE_SYS_SOCKET_H 1 CACHE INTERNAL "Have sys/socket.h header" FORCE)
      set(HAVE_NETDB_H 1 CACHE INTERNAL "Have netdb.h header" FORCE)
      
      if(NOT CURL_USE_SSL)
        set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
        set(HTTP_ONLY ON CACHE BOOL "" FORCE)
        set(CURL_DISABLE_CRYPTO_AUTH ON CACHE BOOL "" FORCE)
      endif()
      
      add_subdirectory(${curl_SOURCE_DIR} ${curl_BINARY_DIR} EXCLUDE_FROM_ALL)
      
      set(CURL_INCLUDE_DIRS ${curl_SOURCE_DIR}/include)
      set(CURL_LIBRARIES libcurl)
    endif()
  endif()
endif()

include_directories(${CURL_INCLUDE_DIRS})

# Common include directories
include_directories(SYSTEM ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/isocline)