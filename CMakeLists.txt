cmake_minimum_required(VERSION 3.10)

# Set the project name
project(TermPassCPP)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17) # Ensure C++17 standard is used
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include the header files directory
include_directories(include)

# Include nlohmann/json library
include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.10.5
)
FetchContent_MakeAvailable(json)

# Add the executable
add_executable(TermPassCPP src/main.cpp src/terminalpassthrough.cpp) # Include terminalpassthrough.cpp

# Link the nlohmann/json library
target_link_libraries(TermPassCPP PRIVATE nlohmann_json::nlohmann_json)
