# CMakeLists.txt
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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

add_executable(
    history_expansion_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/test_history_expansion.cpp"
)
target_link_libraries(history_expansion_tests PRIVATE cjsh_core cjsh_isocline)

add_test(NAME isocline_behavior_tests COMMAND isocline_behavior_tests)
add_test(NAME completion_tests COMMAND completion_tests)
add_test(NAME syntax_highlighting_tests COMMAND syntax_highlighting_tests)
add_test(NAME history_expansion_tests COMMAND history_expansion_tests)

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
