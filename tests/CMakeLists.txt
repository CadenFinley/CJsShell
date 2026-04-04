if(NOT BUILD_TESTING)
    return()
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)

add_executable(isocline_behavior_tests "${CMAKE_CURRENT_SOURCE_DIR}/isocline/test_isocline_behavior.c")
target_link_libraries(isocline_behavior_tests PRIVATE cjsh_isocline)

add_executable(isocline_pty_driver "${CMAKE_CURRENT_SOURCE_DIR}/isocline/isocline_pty_driver.c")
target_link_libraries(isocline_pty_driver PRIVATE cjsh_isocline Threads::Threads)

add_executable(
    completion_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/completions/test_completions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/completions/test_completion_globals.cpp"
)
target_link_libraries(completion_tests PRIVATE cjsh_core cjsh_isocline)

add_executable(
    syntax_highlighting_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/highlighter/test_syntax_highlighting.cpp"
)
target_link_libraries(syntax_highlighting_tests PRIVATE cjsh_core cjsh_isocline)

add_test(NAME isocline_behavior_tests COMMAND isocline_behavior_tests)
add_test(NAME completion_tests COMMAND completion_tests)
add_test(NAME syntax_highlighting_tests COMMAND syntax_highlighting_tests)

if(Python3_Interpreter_FOUND)
    add_test(
        NAME isocline_pty_integration_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/isocline/test_isocline_pty.py"
                $<TARGET_FILE:isocline_pty_driver>
    )

    add_test(
        NAME build_system_configuration_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_system/test_build_system.py"
                "${CMAKE_SOURCE_DIR}"
    )
endif()
