# x86_64-specific configuration

# Define that we're targeting x86_64
add_definitions(-DX86_64)
set(IC_APPLE_SILICON OFF CACHE BOOL "Targeting ARM64 architecture" FORCE)

# x86_64-specific compiler flags
if(APPLE)
  # macOS x86_64 specific settings
  set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for macOS" FORCE)
  
  string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  
  # Add x86_64 flags
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch x86_64")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch x86_64")
  
  # Set minimum macOS version
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)
else()
  # Standard x86_64 settings for Linux/Windows
  add_compile_options(-march=x86-64 -mtune=generic)
endif()

# x86_64 optimizations
add_compile_options(-msse4.2 -mavx)
add_compile_options(-fomit-frame-pointer)

# x86_64-specific library paths
if(APPLE)
  # Intel Homebrew paths
  set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  link_directories("/usr/local/lib")
  include_directories("/usr/local/include")
endif()
