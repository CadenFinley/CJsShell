function(cjsh_collect_include_dirs)
    set(_options)
    set(_one_value_args ROOT_DIR OUT_VAR)
    set(_multi_value_args)
    cmake_parse_arguments(CJSH_DIRS "${_options}" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    if(CJSH_DIRS_ROOT_DIR STREQUAL "")
        message(FATAL_ERROR "cjsh_collect_include_dirs requires ROOT_DIR")
    endif()
    if(CJSH_DIRS_OUT_VAR STREQUAL "")
        message(FATAL_ERROR "cjsh_collect_include_dirs requires OUT_VAR")
    endif()

    file(
        GLOB_RECURSE _header_files
        CONFIGURE_DEPENDS
        "${CJSH_DIRS_ROOT_DIR}/*.h"
        "${CJSH_DIRS_ROOT_DIR}/*.hpp"
    )

    set(_include_dirs "${CJSH_DIRS_ROOT_DIR}")
    foreach(_header_file IN LISTS _header_files)
        get_filename_component(_header_dir "${_header_file}" DIRECTORY)
        list(APPEND _include_dirs "${_header_dir}")
    endforeach()
    list(REMOVE_DUPLICATES _include_dirs)

    set(${CJSH_DIRS_OUT_VAR} "${_include_dirs}" PARENT_SCOPE)
endfunction()

function(cjsh_apply_build_profile)
    set(_options)
    set(_one_value_args TARGET MINIMAL_BUILD)
    set(_multi_value_args)
    cmake_parse_arguments(CJSH_PROFILE "${_options}" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    if(NOT TARGET ${CJSH_PROFILE_TARGET})
        message(FATAL_ERROR "cjsh_apply_build_profile expected an existing TARGET")
    endif()

    target_compile_definitions(
        ${CJSH_PROFILE_TARGET}
        INTERFACE
            "$<$<CONFIG:Debug>:DEBUG=1>"
            "$<$<CONFIG:Debug>:CJSH_ENABLE_DEBUG=1>"
            "$<$<NOT:$<CONFIG:Debug>>:IC_NO_DEBUG_MSG=1>"
    )

    if(CJSH_PROFILE_MINIMAL_BUILD)
        target_compile_definitions(
            ${CJSH_PROFILE_TARGET}
            INTERFACE
                CJSH_MINIMAL_BUILD=1
                CJSH_NO_FANCY_FEATURES=1
                IC_NO_DEBUG_MSG=1
        )
    endif()

    if(MSVC)
        target_compile_options(
            ${CJSH_PROFILE_TARGET}
            INTERFACE
                "$<$<CONFIG:Debug>:/Od>"
                "$<$<CONFIG:Debug>:/Zi>"
                "$<$<CONFIG:Release>:/O2>"
                "$<$<CONFIG:RelWithDebInfo>:/O2>"
                "$<$<CONFIG:RelWithDebInfo>:/Zi>"
                "$<$<CONFIG:MinSizeRel>:/O1>"
        )
    else()
        target_compile_options(
            ${CJSH_PROFILE_TARGET}
            INTERFACE
                "$<$<CONFIG:Debug>:-O0>"
                "$<$<CONFIG:Debug>:-g>"
                "$<$<CONFIG:Debug>:-fsanitize=address>"
                "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
        )
        target_link_options(
            ${CJSH_PROFILE_TARGET}
            INTERFACE
                "$<$<CONFIG:Debug>:-fsanitize=address>"
        )

        if(CJSH_PROFILE_MINIMAL_BUILD)
            target_compile_options(
                ${CJSH_PROFILE_TARGET}
                INTERFACE
                    "$<$<NOT:$<CONFIG:Debug>>:-Oz>"
            )
        else()
            target_compile_options(
                ${CJSH_PROFILE_TARGET}
                INTERFACE
                    "$<$<CONFIG:Release>:-O2>"
                    "$<$<CONFIG:RelWithDebInfo>:-O2>"
                    "$<$<CONFIG:MinSizeRel>:-Oz>"
            )
        endif()
    endif()
endfunction()

function(cjsh_enable_binary_stripping target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "cjsh_enable_binary_stripping expected an existing target")
    endif()

    if(MSVC)
        return()
    endif()

    if(APPLE)
        target_link_options(${target_name} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-Wl,-S>")
    else()
        target_link_options(${target_name} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:-s>")
    endif()
endfunction()
