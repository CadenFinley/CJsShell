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
  "src/prompt/modules/*.cpp"
  "src/theme/*.cpp"
)

# Explicitly list isocline .c files that should be compiled
set(ISOCLINE_MAIN_C 
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
  # "src/isocline/wcwidth.c" - Removed: now using utf8proc for Unicode width calculations
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
  "src/isocline/wcwidth.c"
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
  # ${CMAKE_CURRENT_SOURCE_DIR}/src/isocline/wcwidth.c - Removed: using utf8proc
  PROPERTIES COMPILE_FLAGS "-Wno-error"
)

# Find nlohmann JSON library
find_package(nlohmann_json QUIET)

if(NOT nlohmann_json_FOUND)
  # Fallback: Use FetchContent to download nlohmann JSON (CMake 3.11+) or manual download (CMake 3.10)
  message(STATUS "nlohmann_json package not found, downloading from source")
  
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.11")
    # Use FetchContent for CMake 3.11+
    include(FetchContent)
    FetchContent_Declare(
      nlohmann_json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG v3.11.3
      GIT_SHALLOW TRUE
    )
    
    # Set nlohmann_json options
    set(JSON_BuildTests OFF CACHE BOOL "Disable nlohmann_json testing")
    set(JSON_Install OFF CACHE BOOL "Disable nlohmann_json installation")
    
    FetchContent_MakeAvailable(nlohmann_json)
  else()
    # For CMake 3.10, download the single header version
    set(NLOHMANN_JSON_DIR ${CMAKE_BINARY_DIR}/nlohmann_json)
    set(NLOHMANN_JSON_HEADER ${NLOHMANN_JSON_DIR}/nlohmann/json.hpp)
    
    if(NOT EXISTS ${NLOHMANN_JSON_HEADER})
      message(STATUS "Downloading nlohmann/json single header...")
      file(MAKE_DIRECTORY ${NLOHMANN_JSON_DIR}/nlohmann)
      file(DOWNLOAD 
        https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
        ${NLOHMANN_JSON_HEADER}
        SHOW_PROGRESS
        EXPECTED_HASH SHA256=9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6
      )
    endif()
    
    # Create an interface library to mimic the nlohmann_json target
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_DIR}"
    )
  endif()
  
  # The target nlohmann_json::nlohmann_json is automatically created by FetchContent
endif()

# Find utf8proc library
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(UTF8PROC QUIET utf8proc)
endif()

if(NOT UTF8PROC_FOUND)
  # Fallback: Use ExternalProject to download and build utf8proc (compatible with CMake 3.10)
  message(STATUS "utf8proc package not found, downloading and building from source")
  include(ExternalProject)
  
  # Define the external project
  ExternalProject_Add(
    utf8proc_external
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG v2.10.0
    GIT_SHALLOW TRUE
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DUTF8PROC_ENABLE_TESTING=OFF
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
      -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
      -DCMAKE_INSTALL_LIBDIR=lib
    BUILD_BYPRODUCTS <INSTALL_DIR>/lib/libutf8proc${CMAKE_STATIC_LIBRARY_SUFFIX}
  )
  
  # Get the install directory
  ExternalProject_Get_Property(utf8proc_external install_dir)
  
  # Set variables for linking
  set(UTF8PROC_LIBRARIES ${install_dir}/lib/libutf8proc${CMAKE_STATIC_LIBRARY_SUFFIX})
  set(UTF8PROC_INCLUDE_DIRS ${install_dir}/include)
else()
  message(STATUS "Found utf8proc via pkg-config")
  set(UTF8PROC_LIBRARIES ${UTF8PROC_LIBRARIES})
endif()

# Add required include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/isocline
    ${CMAKE_CURRENT_SOURCE_DIR}/include/builtin
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/include/prompt
    ${CMAKE_CURRENT_SOURCE_DIR}/include/prompt/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
)

# Add utf8proc include directories if built from source
if(NOT UTF8PROC_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${UTF8PROC_INCLUDE_DIRS})
endif()

# Link against libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    pthread
    dl
    nlohmann_json::nlohmann_json
    ${UTF8PROC_LIBRARIES}
)

# Add dependency for external utf8proc if it was built from source
if(NOT UTF8PROC_FOUND)
  add_dependencies(${PROJECT_NAME} utf8proc_external)
endif()

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
  target_include_directories(${PROJECT_NAME}_speed03 PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include/isocline
      ${CMAKE_CURRENT_SOURCE_DIR}/include/builtin
      ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
      ${CMAKE_CURRENT_SOURCE_DIR}/include/prompt
      ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
  )
  
  # Add utf8proc include directories if built from source
  if(NOT UTF8PROC_FOUND)
    target_include_directories(${PROJECT_NAME}_speed03 PRIVATE ${UTF8PROC_INCLUDE_DIRS})
  endif()
  target_link_libraries(${PROJECT_NAME}_speed03 PRIVATE
      pthread
      dl
      nlohmann_json::nlohmann_json
      ${UTF8PROC_LIBRARIES}
  )
  
  # Add dependency for external utf8proc if it was built from source
  if(NOT UTF8PROC_FOUND)
    add_dependencies(${PROJECT_NAME}_speed03 utf8proc_external)
  endif()

    # Speed-optimized binary 02
  add_executable(${PROJECT_NAME}_speed02 ${SOURCES})
  target_compile_options(${PROJECT_NAME}_speed02 PRIVATE -O2)
  target_include_directories(${PROJECT_NAME}_speed02 PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include/isocline
      ${CMAKE_CURRENT_SOURCE_DIR}/include/builtin
      ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
      ${CMAKE_CURRENT_SOURCE_DIR}/include/prompt
      ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
  )
  
  # Add utf8proc include directories if built from source
  if(NOT UTF8PROC_FOUND)
    target_include_directories(${PROJECT_NAME}_speed02 PRIVATE ${UTF8PROC_INCLUDE_DIRS})
  endif()
  target_link_libraries(${PROJECT_NAME}_speed02 PRIVATE
      pthread
      dl
      nlohmann_json::nlohmann_json
      ${UTF8PROC_LIBRARIES}
  )
  
  # Add dependency for external utf8proc if it was built from source
  if(NOT UTF8PROC_FOUND)
    add_dependencies(${PROJECT_NAME}_speed02 utf8proc_external)
  endif()

  # Debug binary with no optimizations
  add_executable(${PROJECT_NAME}_debug ${SOURCES})
  target_compile_options(${PROJECT_NAME}_debug PRIVATE -O0 -g)
  target_include_directories(${PROJECT_NAME}_debug PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include/isocline
      ${CMAKE_CURRENT_SOURCE_DIR}/include/builtin
      ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
      ${CMAKE_CURRENT_SOURCE_DIR}/include/prompt
      ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
  )
  
  # Add utf8proc include directories if built from source
  if(NOT UTF8PROC_FOUND)
    target_include_directories(${PROJECT_NAME}_debug PRIVATE ${UTF8PROC_INCLUDE_DIRS})
  endif()
  target_link_libraries(${PROJECT_NAME}_debug PRIVATE
      pthread
      dl
      nlohmann_json::nlohmann_json
      ${UTF8PROC_LIBRARIES}
  )
  
  # Add dependency for external utf8proc if it was built from source
  if(NOT UTF8PROC_FOUND)
    add_dependencies(${PROJECT_NAME}_debug utf8proc_external)
  endif()
endif()
