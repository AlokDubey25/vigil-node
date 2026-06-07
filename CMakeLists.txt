# CMakeLists.txt ROOT 
cmake_minimum_required(VERSION 3.16)
project(vigil_node)

# Set C++ standard to C++17 for structured bindings support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include_directories(cpp/include)


# sqlite3.c uses POSIX APIs - supress its internal warnings
set_source_files_properties(cpp/vendor/sqlite3.c 
    PROPERTIES COMPILE_FLAGS "-w")

# Link pthreads (req by SQLite on LINUX)
find_package(Threads REQUIRED)

# Main Engine binary
add_executable(vigil 
    cpp/src/main.cpp 
    cpp/src/orderbook.cpp
    cpp/src/database.cpp
    cpp/src/feature_extractor.cpp
    cpp/vendor/sqlite3.c 
)
target_link_libraries(vigil Threads::Threads)

# Map the include directory for header files
target_include_directories(vigil PRIVATE cpp/include)

# 3. Test Binary
# Test orderbook
add_executable(test_orderbook
    tests/cpp/test_orderbook.cpp
    cpp/src/orderbook.cpp
    cpp/vendor/sqlite3.c
)

target_include_directories(test_orderbook PRIVATE       # check this later
    cpp/include
    tests/cpp 
)

# Test database 
add_executable(test_database
    tests/cpp/test_database.cpp
    cpp/src/database.cpp
    cpp/vendor/sqlite3.c
)

target_include_directories(test_database PRIVATE       # check this later
    cpp/include
    tests/cpp 
)


target_link_libraries(test_orderbook Threads::Threads)


# 4. CTest integration to model
enable_testing()
add_test(NAME orderbook_tests COMMAND test_orderbook)
add_test(NAME database_tests COMMAND test_database)
