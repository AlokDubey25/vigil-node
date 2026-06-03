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
    cpp/src/database.cpp
    cpp/vendor/sqlite3.c 
)

# sqlite3.c uses POSIX APIs - supress its internal warnings
set_source_files_properties(cpp/vendor/sqlite3.c 
    PROPERTIES COMPILE_FLAGS "-w")

# Link pthreads (req by SQLite on LINUX)
find_package(Threads REQUIRED)
target_link_libraries(vigil Threads::Threads)


# 2. Map the include directory for header files
target_include_directories(vigil PRIVATE cpp/include)

# 3. Test Binary
add_executable(test_orderbook
    tests/cpp/test_orderbook.cpp
    cpp/src/orderbook.cpp
    cpp/vendor/sqlite3.c
)

target_include_directories(test_orderbook PRIVATE       # check this later
    cpp/include
    tests/cpp 
)

# set_source_files_properties(cpp/vendor/sqlite3.c 
#     cpp/include tests/cpp)
# set_source_files_properties(cpp/vendor/sqlite3.c 
#     PROPERTIES COMPILE_FLAGS "-w")
# target_link_libraries(test_orderbook Threads::Threads)


# 4. CTest integration to model
enable_testing()
add_test(NAME orderbook_tests COMMAND test_orderbook)
