# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(CS6378P2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/algorithm.cpp
    src/io.cpp
    src/serialization.cpp
    tests/main.cpp
)
# Header files
include_directories(include)

# Add executable
add_executable(main ${SOURCES})

