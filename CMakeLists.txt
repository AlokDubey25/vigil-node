cmake_minimum_required(VERSION 3.16)
project(vigil_node)
set(CMAKE_CXX_STANDARD 17)
include_directories(cpp/include)
add_executable(vigil cpp/src/main.cpp)
 