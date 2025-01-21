cmake_minimum_required(VERSION 3.10)

# Set the project name
project(TermPassCPP)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17) # Ensure C++17 standard is used
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include the header files directory
include_directories(include)

# Add the executable
add_executable(TermPassCPP src/main.cpp) 
