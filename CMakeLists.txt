cmake_minimum_required(VERSION 3.16)
project(vigil_node LANGUAGES CXX)

# Set C++ standard to C++17 for structured bindings support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Compile both source files into a single executable named 'vigil'
add_executable(vigil 
    cpp/src/main.cpp 
    cpp/src/orderbook.cpp
)

# 2. Map the include directory for header files
target_include_directories(vigil PRIVATE cpp/include)