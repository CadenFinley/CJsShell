cmake_minimum_required(VERSION 3.10)

if(WIN32)
    message(FATAL_ERROR "CJsShell is currently only supported on Unix-based systems (Linux/macOS)")
endif()

project(CJsShell)

# write the binary straight into the build folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(src/include)

add_subdirectory(isocline)

include(ExternalProject)
include(FetchContent)

function(setup_curl_download)
    message(STATUS "CURL not found - will download and build from source")
    ExternalProject_Add(curl_external
        URL https://curl.se/download/curl-8.1.2.tar.gz
        PREFIX ${CMAKE_BINARY_DIR}/external/curl
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external/curl/install
            -DBUILD_SHARED_LIBS=OFF
            -DCMAKE_USE_OPENSSL=ON
            -DBUILD_TESTING=OFF
        )
    set(CURL_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/external/curl/install/include PARENT_SCOPE)
    set(CURL_LIBRARIES ${CMAKE_BINARY_DIR}/external/curl/install/lib/${CMAKE_STATIC_LIBRARY_PREFIX}curl${CMAKE_STATIC_LIBRARY_SUFFIX} PARENT_SCOPE)
    add_library(CURL::libcurl UNKNOWN IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION ${CURL_LIBRARIES}
        INTERFACE_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIRS})
    add_dependencies(CURL::libcurl curl_external)
endfunction()

find_package(CURL QUIET)
if(NOT CURL_FOUND)
    setup_curl_download()
endif()

find_package(Threads REQUIRED)

set(SOURCE_FILES
    src/main.cpp
    src/terminal.cpp
    src/openaipromptengine.cpp
    src/pluginmanager.cpp
    src/thememanager.cpp
)

include_directories(${CURL_INCLUDE_DIRS})

# find or download nlohmann/json
find_path(JSON_INCLUDE_DIR NAMES nlohmann/json.hpp
    PATHS
      /usr/include
      /usr/local/include
      /opt/include
      /opt/local/include
      ${CMAKE_SOURCE_DIR}/external
)
if(JSON_INCLUDE_DIR)
    message(STATUS "Found nlohmann/json at ${JSON_INCLUDE_DIR}")
    set(JSON_USE_FETCHCONTENT OFF)
else()
    message(STATUS "nlohmann/json.hpp not found, downloading and building via FetchContent")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(nlohmann_json)
    set(JSON_INCLUDE_DIR ${nlohmann_json_SOURCE_DIR}/include)
    set(JSON_USE_FETCHCONTENT ON)
endif()
include_directories(${JSON_INCLUDE_DIR})

find_library(DL_LIBRARY dl)
if(NOT DL_LIBRARY)
    message(FATAL_ERROR "dl library not found")
endif()

# single executable for the host system
add_executable(cjsh ${SOURCE_FILES})

# link everything together
if(TARGET CURL::libcurl)
    target_link_libraries(cjsh CURL::libcurl Threads::Threads ${DL_LIBRARY} isocline)
else()
    target_link_libraries(cjsh ${CURL_LIBRARIES} Threads::Threads ${DL_LIBRARY} isocline)
endif()

if(JSON_USE_FETCHCONTENT)
    target_link_libraries(cjsh PRIVATE nlohmann_json::nlohmann_json)
endif()
