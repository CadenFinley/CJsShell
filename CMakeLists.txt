cmake_minimum_required(VERSION 3.10)

if(WIN32)
    message(FATAL_ERROR "CJsShell is currently only supported on Unix-based systems (Linux/macOS)")
endif()

project(CJsShell)

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
    
    # Create necessary directories before they're referenced
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/external/curl/install/include)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/external/curl/install/lib)
    
    # Check for OpenSSL first
    find_package(OpenSSL QUIET)
    
    if(OPENSSL_FOUND)
        message(STATUS "OpenSSL found - building cURL with SSL support")
        set(SSL_CONFIG_FLAG "--with-ssl")
    else()
        message(WARNING "OpenSSL not found - building cURL WITHOUT SSL support.")
        message(WARNING "To enable SSL support, install OpenSSL development files:")
        message(WARNING "  On Debian/Ubuntu: sudo apt-get install libssl-dev")
        message(WARNING "  On Red Hat/CentOS: sudo yum install openssl-devel")
        message(WARNING "  On macOS with Homebrew: brew install openssl")
        set(SSL_CONFIG_FLAG "--without-ssl")
    endif()
    
    # Define curl library path variables - be explicit about static library path
    set(CURL_LIBRARY_PATH ${CMAKE_BINARY_DIR}/external/curl/install/lib/libcurl.a)
    
    # Use the configure script directly for more control
    ExternalProject_Add(curl_external
        URL https://curl.se/download/curl-8.1.2.tar.gz
        PREFIX ${CMAKE_BINARY_DIR}/external/curl
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CONFIGURE_COMMAND <SOURCE_DIR>/configure 
            --prefix=${CMAKE_BINARY_DIR}/external/curl/install
            --disable-shared
            --enable-static
            --disable-ldap
            --disable-ftp
            --disable-file
            --disable-rtsp
            --disable-telnet
            --disable-tftp
            --disable-dict
            --disable-ldaps
            --disable-ldap
            ${SSL_CONFIG_FLAG}
        BUILD_COMMAND make -j4
        INSTALL_COMMAND make install
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS ${CURL_LIBRARY_PATH}
    )
    
    # Create a custom target that depends on curl_external
    add_custom_target(curl_lib ALL DEPENDS curl_external)
    
    # Create a verification target to check if the library exists
    add_custom_command(
        TARGET curl_lib POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Verifying cURL library at ${CURL_LIBRARY_PATH}"
        # Use cat to verify file exists - will fail if not found
        COMMAND ${CMAKE_COMMAND} -E cat ${CURL_LIBRARY_PATH}
        COMMAND ${CMAKE_COMMAND} -E echo "cURL library successfully built and installed"
        COMMENT "Checking cURL library installation"
    )
    
    set(CURL_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/external/curl/install/include PARENT_SCOPE)
    set(CURL_LIBRARIES ${CURL_LIBRARY_PATH} PARENT_SCOPE)
    set(CURL_EXTERNAL_PROJECT curl_external PARENT_SCOPE)
    set(CURL_CUSTOM_TARGET curl_lib PARENT_SCOPE)
endfunction()

find_package(CURL QUIET)
if(NOT CURL_FOUND)
    setup_curl_download()
    add_library(CURL::libcurl UNKNOWN IMPORTED GLOBAL)
    
    # Now the include directory exists before we reference it
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION ${CURL_LIBRARIES}
        INTERFACE_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIRS})
    
    # Add a dependency to ensure the imported target has the actual library file
    if(TARGET curl_lib)
        add_dependencies(CURL::libcurl curl_lib)
    endif()
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

# Function to download and setup nlohmann/json
function(setup_json_download)
    message(STATUS "nlohmann/json not found - will download from source")
    
    # Create necessary directory before referencing it
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/external/json/include)
    
    ExternalProject_Add(json_external
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
        PREFIX ${CMAKE_BINARY_DIR}/external/json
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CONFIGURE_COMMAND ""  # No configure step needed
        BUILD_COMMAND ""      # No build step needed
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
            <SOURCE_DIR>/include
            ${CMAKE_BINARY_DIR}/external/json/include
    )
    set(JSON_INCLUDE_DIR ${CMAKE_BINARY_DIR}/external/json/include PARENT_SCOPE)
    set(JSON_EXTERNAL_PROJECT json_external PARENT_SCOPE)
endfunction()

find_path(JSON_INCLUDE_DIR NAMES nlohmann/json.hpp
    PATHS
    /usr/include
    /usr/local/include
    /opt/include
    /opt/local/include
    ${CMAKE_SOURCE_DIR}/external
)

# Set a variable to track if we need to add JSON dependency later
set(NEEDS_JSON_DEPENDENCY FALSE)

if(JSON_INCLUDE_DIR)
    message(STATUS "Found nlohmann/json: ${JSON_INCLUDE_DIR}")
    include_directories(${JSON_INCLUDE_DIR})
else()
    message(STATUS "nlohmann/json.hpp not found in standard locations")
    setup_json_download()
    include_directories(${JSON_INCLUDE_DIR})
    
    # Set flag to add dependency later after targets are created
    if(TARGET json_external)
        set(NEEDS_JSON_DEPENDENCY TRUE)
    endif()
endif()

find_library(DL_LIBRARY dl)
if(NOT DL_LIBRARY)
    message(FATAL_ERROR "dl library not found")
endif()

# Add macOS target if on Apple platform
if(APPLE)
    add_executable(CJsShell_macOS ${SOURCE_FILES})
    set_target_properties(CJsShell_macOS PROPERTIES
        OUTPUT_NAME "cjsh-macos"
        RUNTIME_OUTPUT_DIRECTORY ${MACOS_OUTPUT_DIR}
    )
    
    if(TARGET CURL::libcurl)
        target_link_libraries(CJsShell_macOS CURL::libcurl Threads::Threads ${DL_LIBRARY} isocline)
        if(TARGET curl_external)
            add_dependencies(CJsShell_macOS curl_external)
        endif()
    else()
        target_link_libraries(CJsShell_macOS ${CURL_LIBRARIES} Threads::Threads ${DL_LIBRARY} isocline)
    endif()
    
    if(Readline_FOUND)
        target_link_libraries(CJsShell_macOS ${Readline_LIBRARY})
    endif()
endif()

# For the Linux target
add_executable(CJsShell_Linux ${SOURCE_FILES})
set_target_properties(CJsShell_Linux PROPERTIES
    OUTPUT_NAME "cjsh-linux"
    RUNTIME_OUTPUT_DIRECTORY ${LINUX_OUTPUT_DIR}
)

if(TARGET CURL::libcurl)
    target_link_libraries(CJsShell_Linux CURL::libcurl Threads::Threads ${DL_LIBRARY} isocline)
    # Make sure the binary depends on curl_lib, not just curl_external
    if(TARGET curl_lib)
        add_dependencies(CJsShell_Linux curl_lib)
    endif()
else()
    target_link_libraries(CJsShell_Linux ${CURL_LIBRARIES} Threads::Threads ${DL_LIBRARY} isocline)
endif()

# Now that targets are defined, add JSON dependency if needed
if(NEEDS_JSON_DEPENDENCY)
    if(TARGET json_external)
        add_dependencies(CJsShell_Linux json_external)
        if(APPLE AND TARGET CJsShell_macOS)
            add_dependencies(CJsShell_macOS json_external)
        endif()
    endif()
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
