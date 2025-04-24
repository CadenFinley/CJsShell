cmake_minimum_required(VERSION 3.10)
project(CJsShell_Plugins)

# ensure sub‑projects see at least our CMake version
cmake_policy(VERSION 3.10)

# Set output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set shared library properties
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Platform-specific settings
if(APPLE)
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".dylib")
    set(CMAKE_MACOSX_RPATH ON)
elseif(UNIX)
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
endif()

# Include parent project directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
)

# Add nlohmann/json library using FetchContent
include(FetchContent)
# allow the old Populate call without dev‑warning
cmake_policy(SET CMP0169 OLD)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2  # Use a specific version for stability
)
FetchContent_GetProperties(json)
if(NOT json_POPULATED)
    FetchContent_Populate(json)
    file(READ "${json_SOURCE_DIR}/CMakeLists.txt" _json_cmake)
    # replace any first cmake_minimum_required(...) with our 3.5 threshold
    string(REGEX REPLACE
        "^cmake_minimum_required\\([^\\)]*\\)"
        "cmake_minimum_required(VERSION 3.5)"
        _json_cmake
        "${_json_cmake}"
    )
    file(WRITE "${json_SOURCE_DIR}/CMakeLists.txt" "${_json_cmake}")
endif()
add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR})

# Find libcurl for plugins that need HTTP functionality
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Find OpenSSL for plugins that need cryptographic functionality
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Build plugins directly from this directory
file(GLOB PLUGIN_SOURCES "*.cpp")
message(STATUS "Found plugin source files: ${PLUGIN_SOURCES}")

if(PLUGIN_SOURCES)
    foreach(plugin_source ${PLUGIN_SOURCES})
        # Extract the filename without extension to use as target name
        get_filename_component(plugin_name ${plugin_source} NAME_WE)
        
        message(STATUS "Building plugin: ${plugin_name} from ${plugin_source}")
        
        # Create shared library for this plugin
        add_library(${plugin_name} SHARED ${plugin_source})
        
        # Set properties for the plugin
        set_target_properties(${plugin_name} PROPERTIES
            PREFIX ""
        )
        
        # Read the plugin source file content
        file(READ ${plugin_source} plugin_content)
        
        # Check if this plugin uses nlohmann/json
        string(FIND "${plugin_content}" "nlohmann/json" json_dependency_pos)
        if(NOT ${json_dependency_pos} EQUAL -1)
            message(STATUS "Plugin ${plugin_name} uses nlohmann/json, linking library")
            target_link_libraries(${plugin_name} PRIVATE nlohmann_json::nlohmann_json)
        endif()
        
        # Check if this plugin uses libcurl
        string(FIND "${plugin_content}" "<curl/curl.h>" curl_dependency_pos)
        if(NOT ${curl_dependency_pos} EQUAL -1)
            message(STATUS "Plugin ${plugin_name} uses libcurl, linking library")
            target_link_libraries(${plugin_name} PRIVATE ${CURL_LIBRARIES})
        endif()
        
        # Check if this plugin uses OpenSSL
        string(FIND "${plugin_content}" "<openssl/" openssl_dependency_pos)
        if(NOT ${openssl_dependency_pos} EQUAL -1)
            message(STATUS "Plugin ${plugin_name} uses OpenSSL, linking library")
            target_link_libraries(${plugin_name} PRIVATE ${OPENSSL_LIBRARIES})
        endif()
    endforeach()
else()
    message(WARNING "No .cpp files found in plugins directory")
endif()

# Add a message to confirm the script has run completely
message(STATUS "CJsShell_Plugins configuration complete")
