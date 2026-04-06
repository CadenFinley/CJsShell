function(cjsh_escape_define_value out_var value)
    set(_escaped_value "${value}")
    string(REPLACE "\\" "\\\\" _escaped_value "${_escaped_value}")
    string(REPLACE "\"" "\\\"" _escaped_value "${_escaped_value}")
    string(REPLACE ";" "\\;" _escaped_value "${_escaped_value}")
    set(${out_var} "${_escaped_value}" PARENT_SCOPE)
endfunction()

function(cjsh_compute_build_metadata)
    set(_options)
    set(_one_value_args
        OUT_GIT_HASH
        OUT_GIT_HASH_FULL
        OUT_BUILD_ARCH
        OUT_BUILD_PLATFORM
        OUT_BUILD_TIME
        OUT_BUILD_COMPILER
        OUT_CXX_STANDARD
        OUT_BUILD_TYPE
    )
    set(_multi_value_args)
    cmake_parse_arguments(CJSH_META "${_options}" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    set(_git_hash_override "$ENV{CJSH_GIT_HASH_OVERRIDE}")
    if(NOT _git_hash_override STREQUAL "")
        set(_git_hash_short "${_git_hash_override}")
        set(_git_hash_full "${_git_hash_override}")
    else()
        set(_git_hash_short "unknown")
        set(_git_hash_full "unknown")
        set(_is_dirty OFF)

        find_package(Git QUIET)
        if(Git_FOUND)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE _git_hash_short_output
                RESULT_VARIABLE _git_hash_short_status
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_git_hash_short_status EQUAL 0 AND NOT _git_hash_short_output STREQUAL "")
                set(_git_hash_short "${_git_hash_short_output}")
            endif()

            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE _git_hash_full_output
                RESULT_VARIABLE _git_hash_full_status
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_git_hash_full_status EQUAL 0 AND NOT _git_hash_full_output STREQUAL "")
                set(_git_hash_full "${_git_hash_full_output}")
            endif()

            execute_process(
                COMMAND "${GIT_EXECUTABLE}" status --porcelain
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE _git_status_output
                RESULT_VARIABLE _git_status_status
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_git_status_status EQUAL 0)
                string(STRIP "${_git_status_output}" _git_status_output)
                if(NOT _git_status_output STREQUAL "")
                    set(_is_dirty ON)
                endif()
            endif()
        endif()

        if(_is_dirty)
            if(NOT _git_hash_short STREQUAL "unknown")
                set(_git_hash_short "${_git_hash_short}-dirty")
            endif()
            if(NOT _git_hash_full STREQUAL "unknown")
                set(_git_hash_full "${_git_hash_full}-dirty")
            endif()
        endif()
    endif()

    string(TIMESTAMP _build_time "%Y-%m-%dT%H:%M:%SZ" UTC)

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(_build_platform "apple-darwin")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_build_platform "linux")
    elseif(CMAKE_SYSTEM_NAME MATCHES "^[Ww]indows")
        set(_build_platform "windows")
    else()
        set(_build_platform "unix")
    endif()

    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _processor_lower)
    if(_processor_lower MATCHES "^(aarch64|arm64)$")
        set(_build_arch "arm64")
    elseif(_processor_lower MATCHES "^(x86_64|amd64)$")
        set(_build_arch "x86_64")
    elseif(_processor_lower MATCHES "^(i[3-6]86|x86)$")
        set(_build_arch "x86")
    elseif(_processor_lower MATCHES "(ppc|power)")
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(_build_arch "ppc")
        else()
            set(_build_arch "ppc64")
        endif()
    else()
        set(_build_arch "unknown")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "" AND CMAKE_CXX_COMPILER_VERSION STREQUAL "")
        set(_build_compiler "unknown")
    elseif(CMAKE_CXX_COMPILER_VERSION STREQUAL "")
        set(_build_compiler "${CMAKE_CXX_COMPILER_ID}")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "")
        set(_build_compiler "${CMAKE_CXX_COMPILER_VERSION}")
    else()
        set(_build_compiler "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    endif()

    if(CMAKE_CXX_STANDARD)
        set(_cxx_standard "${CMAKE_CXX_STANDARD}")
    else()
        set(_cxx_standard "17")
    endif()

    if(CMAKE_CONFIGURATION_TYPES)
        set(_build_type "$<CONFIG>")
    else()
        set(_build_type "${CMAKE_BUILD_TYPE}")
        if(_build_type STREQUAL "")
            set(_build_type "Release")
        endif()
    endif()

    set(${CJSH_META_OUT_GIT_HASH} "${_git_hash_short}" PARENT_SCOPE)
    set(${CJSH_META_OUT_GIT_HASH_FULL} "${_git_hash_full}" PARENT_SCOPE)
    set(${CJSH_META_OUT_BUILD_ARCH} "${_build_arch}" PARENT_SCOPE)
    set(${CJSH_META_OUT_BUILD_PLATFORM} "${_build_platform}" PARENT_SCOPE)
    set(${CJSH_META_OUT_BUILD_TIME} "${_build_time}" PARENT_SCOPE)
    set(${CJSH_META_OUT_BUILD_COMPILER} "${_build_compiler}" PARENT_SCOPE)
    set(${CJSH_META_OUT_CXX_STANDARD} "${_cxx_standard}" PARENT_SCOPE)
    set(${CJSH_META_OUT_BUILD_TYPE} "${_build_type}" PARENT_SCOPE)
endfunction()

function(cjsh_apply_metadata_definitions)
    set(_options)
    set(_one_value_args
        TARGET
        GIT_HASH
        GIT_HASH_FULL
        BUILD_ARCH
        BUILD_PLATFORM
        BUILD_TIME
        BUILD_COMPILER
        CXX_STANDARD
        BUILD_TYPE
        VERSION_BASE
        PRE_RELEASE
    )
    set(_multi_value_args)
    cmake_parse_arguments(CJSH_META "${_options}" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    if(NOT TARGET ${CJSH_META_TARGET})
        message(FATAL_ERROR "cjsh_apply_metadata_definitions expected an existing TARGET")
    endif()

    cjsh_escape_define_value(_git_hash "${CJSH_META_GIT_HASH}")
    cjsh_escape_define_value(_git_hash_full "${CJSH_META_GIT_HASH_FULL}")
    cjsh_escape_define_value(_build_arch "${CJSH_META_BUILD_ARCH}")
    cjsh_escape_define_value(_build_platform "${CJSH_META_BUILD_PLATFORM}")
    cjsh_escape_define_value(_build_time "${CJSH_META_BUILD_TIME}")
    cjsh_escape_define_value(_build_compiler "${CJSH_META_BUILD_COMPILER}")
    cjsh_escape_define_value(_cxx_standard "${CJSH_META_CXX_STANDARD}")
    cjsh_escape_define_value(_build_type "${CJSH_META_BUILD_TYPE}")

    if(CJSH_META_VERSION_BASE STREQUAL "")
        set(CJSH_META_VERSION_BASE "0.0.0")
    endif()
    cjsh_escape_define_value(_version_base "${CJSH_META_VERSION_BASE}")

    if(CJSH_META_PRE_RELEASE)
        set(_pre_release "1")
    else()
        set(_pre_release "0")
    endif()

    target_compile_definitions(
        ${CJSH_META_TARGET}
        INTERFACE
            "CJSH_GIT_HASH=\"${_git_hash}\""
            "CJSH_GIT_HASH_FULL=\"${_git_hash_full}\""
            "CJSH_BUILD_ARCH=\"${_build_arch}\""
            "CJSH_BUILD_PLATFORM=\"${_build_platform}\""
            "CJSH_BUILD_TIME=\"${_build_time}\""
            "CJSH_BUILD_COMPILER=\"${_build_compiler}\""
            "CJSH_CXX_STANDARD=\"${_cxx_standard}\""
            "CJSH_BUILD_TYPE=\"${_build_type}\""
            "CJSH_VERSION_BASE=\"${_version_base}\""
            "CJSH_PRE_RELEASE=${_pre_release}"
    )
endfunction()
