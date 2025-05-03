cmake_minimum_required(VERSION 3.5)
project(CJSH_Tests)

# point to your root includes and sources
include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/vendor)
add_executable(parser_tests parser_tests.cpp)
target_link_libraries(parser_tests PRIVATE pthread)  # if needed
enable_testing()
add_test(NAME parser_tests COMMAND parser_tests)
