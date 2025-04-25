cmake_minimum_required(VERSION 3.10)

if(WIN32)
    message(FATAL_ERROR "CJsShell is currently only supported on Unix-based systems (Linux/macOS)")
endif()

project(CJsShell)

option(BUILD_FOR_BREW "Enable build configuration for Homebrew" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Architecture handling
if(APPLE)
    # Determine host architecture
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE HOST_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # Set macOS specific directories and options
    set(MACOS_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/macos)
    
    # Architecture options
    option(BUILD_UNIVERSAL "Build universal binary for macOS (both arm64 and x86_64)" OFF)
    set(TARGET_ARCH "${HOST_ARCH}" CACHE STRING "Target architecture (arm64, x86_64, or both for universal)")
    
    # Configure architecture settings based on options
    if(BUILD_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary with both architectures" FORCE)
        message(STATUS "Building universal binary for macOS with architectures: arm64;x86_64")
    elseif(TARGET_ARCH STREQUAL "both")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary with both architectures" FORCE)
        message(STATUS "Building universal binary for macOS with architectures: arm64;x86_64")
    elseif(TARGET_ARCH STREQUAL "arm64" OR TARGET_ARCH STREQUAL "x86_64")
        set(CMAKE_OSX_ARCHITECTURES "${TARGET_ARCH}" CACHE STRING "Build architecture for macOS" FORCE)
        message(STATUS "Building for macOS with architecture: ${TARGET_ARCH}")
    else()
        # Default to host architecture if no valid option is provided
        set(CMAKE_OSX_ARCHITECTURES "${HOST_ARCH}" CACHE STRING "Build architecture for macOS" FORCE)
        message(STATUS "Building for macOS with host architecture: ${HOST_ARCH}")
    endif()
    
    # Set minimum deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")
endif()

set(LINUX_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Display architecture info for debugging
if(APPLE)
    message(STATUS "macOS build configuration:")
    message(STATUS "  Target architecture(s): ${CMAKE_OSX_ARCHITECTURES}")
    message(STATUS "  Deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

include_directories(src/include)

add_subdirectory(isocline)

include(ExternalProject)

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

find_package(Readline QUIET)
if(NOT Readline_FOUND)
    find_path(Readline_INCLUDE_DIR readline/readline.h
        PATHS /usr/include /usr/local/include /opt/include /opt/local/include)
    find_library(Readline_LIBRARY NAMES readline
        PATHS /usr/lib /usr/local/lib /opt/lib /opt/local/lib)
    
    if(Readline_INCLUDE_DIR AND Readline_LIBRARY)
        set(Readline_FOUND TRUE)
        message(STATUS "Found readline: ${Readline_LIBRARY}")
    else()
        message(STATUS "Readline library not found - using isocline instead")
        set(Readline_FOUND FALSE)
    endif()
endif()

if(Readline_FOUND)
    include_directories(${Readline_INCLUDE_DIR})
endif()

set(SOURCE_FILES
    src/main.cpp
    src/terminal.cpp
    src/openaipromptengine.cpp
    src/pluginmanager.cpp
    src/thememanager.cpp
)

include_directories(${CURL_INCLUDE_DIRS})

find_path(JSON_INCLUDE_DIR NAMES nlohmann/json.hpp
    PATHS
    /usr/include
    /usr/local/include
    /opt/include
    /opt/local/include
    ${CMAKE_SOURCE_DIR}/external
)

if(JSON_INCLUDE_DIR)
    include_directories(${JSON_INCLUDE_DIR})
else()
    message(WARNING "nlohmann/json.hpp not found, using fallback include location")
    include_directories("/usr/include/nlohmann")
endif()

find_library(DL_LIBRARY dl)
if(NOT DL_LIBRARY)
    message(FATAL_ERROR "dl library not found")
endif()

if(BUILD_FOR_BREW)
    add_executable(cjsh ${SOURCE_FILES})
    if(TARGET CURL::libcurl)
        target_link_libraries(cjsh CURL::libcurl Threads::Threads ${DL_LIBRARY} isocline)
    else()
        target_link_libraries(cjsh ${CURL_LIBRARIES} Threads::Threads ${DL_LIBRARY} isocline)
    endif()
    if(Readline_FOUND)
        target_link_libraries(cjsh ${Readline_LIBRARY})
    endif()
    install(TARGETS cjsh RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    if(APPLE)
        add_executable(CJsShell_macOS ${SOURCE_FILES})
        set_target_properties(CJsShell_macOS PROPERTIES
            OUTPUT_NAME "cjsh-macos"
            RUNTIME_OUTPUT_DIRECTORY ${MACOS_OUTPUT_DIR}
        )
        
        message(STATUS "Building for macOS with architecture: ${CMAKE_OSX_ARCHITECTURES}")
        
        if(TARGET CURL::libcurl)
            target_link_libraries(CJsShell_macOS CURL::libcurl Threads::Threads isocline)
        else()
            target_link_libraries(CJsShell_macOS ${CURL_LIBRARIES} Threads::Threads isocline)
        endif()
        
        if(Readline_FOUND)
            target_link_libraries(CJsShell_macOS ${Readline_LIBRARY})
        endif()
        
        add_custom_command(TARGET CJsShell_macOS POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Built macOS binary with architecture: ${CMAKE_OSX_ARCHITECTURES}"
            COMMAND file ${MACOS_OUTPUT_DIR}/cjsh-macos
        )
    endif()

    add_executable(CJsShell_Linux ${SOURCE_FILES})
    set_target_properties(CJsShell_Linux PROPERTIES
        OUTPUT_NAME "cjsh-linux"
        RUNTIME_OUTPUT_DIRECTORY ${LINUX_OUTPUT_DIR}
    )

    if(TARGET CURL::libcurl)
        target_link_libraries(CJsShell_Linux CURL::libcurl Threads::Threads ${DL_LIBRARY} isocline)
    else()
        target_link_libraries(CJsShell_Linux ${CURL_LIBRARIES} Threads::Threads ${DL_LIBRARY} isocline)
    endif()

    if(Readline_FOUND)
        target_link_libraries(CJsShell_Linux ${Readline_LIBRARY})
    endif()

    if(UNIX AND NOT APPLE)
        set_target_properties(CJsShell_Linux PROPERTIES
            INSTALL_RPATH "$ORIGIN/../lib"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()

    if(STATIC_LINKING)
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a .so)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()

    include(InstallRequiredSystemLibraries)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_PACKAGE_VERSION_MAJOR "1")
    set(CPACK_PACKAGE_VERSION_MINOR "0")
    set(CPACK_PACKAGE_VERSION_PATCH "0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CJ's Shell - A developer tools terminal")
    set(CPACK_PACKAGE_VENDOR "CJsShell")

    include(CPack)

    install(TARGETS CJsShell_Linux
        RUNTIME DESTINATION bin/linux
        CONFIGURATIONS Release
    )

    if(APPLE)
        install(TARGETS CJsShell_macOS
            RUNTIME DESTINATION bin/macos
            CONFIGURATIONS Release
        )
    endif()
endif()
