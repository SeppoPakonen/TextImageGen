cmake_minimum_required(VERSION 3.10)
project(txt2png)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig)

# Define executable
add_executable(txt2png txt2png.cpp)

# Add compile options
target_compile_options(txt2png PRIVATE -Wall -Wextra -O2)

# On non-Windows systems, we need to ensure the executable can run
if(NOT WIN32)
    target_compile_definitions(txt2png PRIVATE _GNU_SOURCE)
endif()

# Installation
install(TARGETS txt2png DESTINATION bin)