# CMakeLists.txt ROOT 
cmake_minimum_required(VERSION 3.16)
project(vigil_node)

# Set C++ standard to C++17 for structured bindings support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include_directories(cpp/include)

# 1. Main Engine binary
add_executable(vigil 
    cpp/src/main.cpp 
    cpp/src/orderbook.cpp
)

# 2. Map the include directory for header files
target_include_directories(vigil PRIVATE cpp/include)

# 3. Test Binary
add_executable(test_orderbook
    tests/cpp/test_orderbook.cpp
    cpp/src/orderbook.cpp
)
target_include_directories(test_orderbook PRIVATE 
    cpp/include
    tests/cpp 
)

# 4. CTest integration to model
enable_testing()
add_test(NAME orderbook_tests COMMAND test_orderbook)
