# Fetch and build GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include the project source directory
include_directories(${CMAKE_SOURCE_DIR}/src)

# Add test executables
add_executable(plugin_manager_test plugin_manager_test.cpp)
add_executable(theme_manager_test theme_manager_test.cpp)
add_executable(terminal_passthrough_test terminal_passthrough_test.cpp)

# Link the test executables with Google Test and the project libraries
target_link_libraries(plugin_manager_test PUBLIC gtest_main)
target_link_libraries(theme_manager_test PUBLIC gtest_main)
target_link_libraries(terminal_passthrough_test PUBLIC gtest_main)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(plugin_manager_test)
gtest_discover_tests(theme_manager_test)
gtest_discover_tests(terminal_passthrough_test)
