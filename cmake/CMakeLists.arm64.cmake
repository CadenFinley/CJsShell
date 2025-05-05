# ARM64-specific configuration

# Define that we're targeting ARM64
add_compile_definitions(IC_ARM64)
set(IC_APPLE_SILICON ON CACHE BOOL "Targeting ARM64 architecture" FORCE)

# ARM64-specific compiler flags
if(APPLE)
  # Apple Silicon specific settings
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
  
  # Remove any incompatible CPU flags
  string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  
  string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  
  # Add ARM64 flags
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch arm64")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch arm64")
  
  # Set minimum macOS version for ARM64
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
  
  # Add Apple Silicon specific include paths for C++ standard library
  include_directories(SYSTEM "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1")
else()
  # Non-Apple ARM64 settings
  add_compile_options(-march=armv8-a)
endif()

# ARM64 optimizations
add_compile_options(-ftree-vectorize)
add_compile_options(-fomit-frame-pointer)

# ARM64-specific library paths
if(APPLE)
  # Homebrew on ARM64 uses different paths
  set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  link_directories("/opt/homebrew/lib")
  include_directories("/opt/homebrew/include")
endif()
