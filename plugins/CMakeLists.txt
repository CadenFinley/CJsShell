cmake_minimum_required(VERSION 3.10)
project(CJsShell_Plugins)
cmake_policy(VERSION 3.10)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Architecture detection and handling - sync with parent CMakeLists.txt
execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Plugin build - detected architecture: ${ARCH}")

# Clear any potentially problematic CPU-specific flags
string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE "-mcpu=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE "-march=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "-mtune=[^ ]+" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE "-mtune=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

if(APPLE AND ARCH STREQUAL "arm64")
    # Apple Silicon (arm64) - use simple, safe optimization
    message(STATUS "Plugin build - configuring for Apple Silicon")
    add_compile_options(-O3 -arch arm64)
else()
    # For other architectures, use safe optimization
    add_compile_options(-O3)
endif()

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".dylib")
    set(CMAKE_MACOSX_RPATH ON)
elseif(UNIX)
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
endif()
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/include
)
include(FetchContent)
cmake_policy(SET CMP0169 OLD)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_GetProperties(json)
if(NOT json_POPULATED)
    FetchContent_Populate(json)
    file(READ "${json_SOURCE_DIR}/CMakeLists.txt" _json_cmake)
    string(REGEX REPLACE
        "^cmake_minimum_required\\([^\\)]*\\)"
        "cmake_minimum_required(VERSION 3.5)"
        _json_cmake
        "${_json_cmake}"
    )
    file(WRITE "${json_SOURCE_DIR}/CMakeLists.txt" "${_json_cmake}")
endif()
add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR})
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
file(GLOB PLUGIN_SOURCES "*.cpp")
message(STATUS "Found plugin source files: ${PLUGIN_SOURCES}")
if(PLUGIN_SOURCES)
    foreach(plugin_source ${PLUGIN_SOURCES})
        get_filename_component(plugin_name ${plugin_source} NAME_WE)
        message(STATUS "Building plugin: ${plugin_name} from ${plugin_source}")
        add_library(${plugin_name} SHARED ${plugin_source})
        set_target_properties(${plugin_name} PROPERTIES
            PREFIX ""
        )
        file(READ ${plugin_source} plugin_content)
        string(FIND "${plugin_content}" "nlohmann/json" json_dependency_pos)
        if(NOT ${json_dependency_pos} EQUAL -1)
            message(STATUS "Plugin ${plugin_name} uses nlohmann/json, linking library")
            target_link_libraries(${plugin_name} PRIVATE nlohmann_json::nlohmann_json)
        endif()
        string(FIND "${plugin_content}" "<curl/curl.h>" curl_dependency_pos)
        if(NOT ${curl_dependency_pos} EQUAL -1)
            message(STATUS "Plugin ${plugin_name} uses libcurl, linking library")
            target_link_libraries(${plugin_name} PRIVATE ${CURL_LIBRARIES})
        endif()
        string(FIND "${plugin_content}" "<openssl/" openssl_dependency_pos)
        if(NOT ${openssl_dependency_pos} EQUAL -1)
            message(STATUS "Plugin ${plugin_name} uses OpenSSL, linking library")
            target_link_libraries(${plugin_name} PRIVATE ${OPENSSL_LIBRARIES})
        endif()
    endforeach()
else()
    message(WARNING "No .cpp files found in plugins directory")
endif()
message(STATUS "CJsShell_Plugins configuration complete")
