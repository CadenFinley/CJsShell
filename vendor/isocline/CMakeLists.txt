cmake_minimum_required(VERSION 3.16)

project(isocline VERSION 1.0.9 LANGUAGES C)

set(default_build_type "Release")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type specified; defaulting to ${default_build_type}.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "Available build configurations" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

option(ISOCLINE_BUILD_EXAMPLE "Build the CLI example program" ON)
option(ISOCLINE_BUILD_TEST_COLORS "Build the color diagnostic utility" ON)

# Build the library from the single translation unit for optimal
# size and cross-module optimizations.
set(ISOCLINE_SOURCES
    src/isocline.c
)

add_library(isocline STATIC ${ISOCLINE_SOURCES})
add_library(isocline::isocline ALIAS isocline)

target_include_directories(isocline
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(isocline PUBLIC c_std_99)
set_target_properties(isocline PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)

set(ISOCLINE_RELEASE_COMPILE_OPTIONS)
set(ISOCLINE_RELEASE_LINK_OPTIONS)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  list(APPEND ISOCLINE_RELEASE_COMPILE_OPTIONS -O3 -ffunction-sections -fdata-sections)
  if(APPLE)
    list(APPEND ISOCLINE_RELEASE_LINK_OPTIONS -Wl,-dead_strip)
  else()
    list(APPEND ISOCLINE_RELEASE_LINK_OPTIONS -Wl,--gc-sections)
  endif()
elseif(MSVC)
  list(APPEND ISOCLINE_RELEASE_COMPILE_OPTIONS /O2 /GL)
  list(APPEND ISOCLINE_RELEASE_LINK_OPTIONS /LTCG)
endif()

if(ISOCLINE_RELEASE_COMPILE_OPTIONS)
  target_compile_options(isocline PRIVATE $<$<CONFIG:Release>:${ISOCLINE_RELEASE_COMPILE_OPTIONS}>)
endif()
if(ISOCLINE_RELEASE_LINK_OPTIONS)
  target_link_options(isocline PRIVATE $<$<CONFIG:Release>:${ISOCLINE_RELEASE_LINK_OPTIONS}>)
endif()

if(ISOCLINE_BUILD_EXAMPLE)
  add_executable(example test/example.c)
  target_link_libraries(example PRIVATE isocline)
  target_include_directories(example PRIVATE include)
endif()

if(ISOCLINE_BUILD_TEST_COLORS)
  add_executable(test_colors test/test_colors.c)
  target_link_libraries(test_colors PRIVATE isocline)
  target_include_directories(test_colors PRIVATE include)
endif()
