cmake_minimum_required(VERSION 3.10)
project(plugin_test CXX)

set(CMAKE_CXX_STANDARD 17)

# Include the core headers for plugin API
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Build shared library for the plugin
add_library(plugin_test SHARED plugin_test.cpp)

# On macOS, create a .dylib; on other systems, .so
if(APPLE)
  # Use .dylib extension and allow undefined symbols to be resolved at runtime
  set_target_properties(plugin_test PROPERTIES
    SUFFIX ".dylib"
    # Permit undefined symbols (from host application) during linking
    LINK_FLAGS "-undefined dynamic_lookup -flat_namespace"
  )
else()
  set_target_properties(plugin_test PROPERTIES SUFFIX ".so")
endif()

# Remove default 'lib' prefix
set_target_properties(plugin_test PROPERTIES PREFIX "")