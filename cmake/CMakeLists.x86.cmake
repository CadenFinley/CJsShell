# x86 (32-bit) specific configuration

# Define that we're targeting x86
add_definitions(-DX86)
set(IC_APPLE_SILICON OFF CACHE BOOL "Targeting ARM64 architecture" FORCE)

# x86 specific compiler flags
if(APPLE)
  # macOS 32-bit specific settings (note: newer macOS versions don't support 32-bit)
  set(CMAKE_OSX_ARCHITECTURES "i386" CACHE STRING "Build architectures for macOS" FORCE)
  
  string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  
  # Add x86 flags
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch i386 -m32")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch i386 -m32")
  
  # Set minimum macOS version that supports 32-bit
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
else()
  # Linux/Windows 32-bit settings
  add_compile_options(-m32 -march=i686 -mtune=generic)
  
  # Ensure linking is also done with 32-bit flags
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
  
  # Use proper library paths
  link_directories(/usr/lib32 /lib32)
endif()

# x86 optimizations (careful with these on 32-bit)
add_compile_options(-fomit-frame-pointer)

# x86 specific library paths for Linux
if(UNIX AND NOT APPLE)
  # Add the 32-bit library paths
  set(ENV{PKG_CONFIG_PATH} "/usr/lib/i386-linux-gnu/pkgconfig:/usr/lib32/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  include_directories("/usr/include/i386-linux-gnu")
endif()
