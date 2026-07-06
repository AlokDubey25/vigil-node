# CMakeLists.txt ROOT 
cmake_minimum_required(VERSION 3.16)
project(vigil_node)

# Set C++ standard to C++17 for structured bindings support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include_directories(cpp/include)

# helper to fail early with a clear message when a source file is missing
macro(check_required_file relpath)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relpath}")
    message(FATAL_ERROR "Required source file '${relpath}' not found. Ensure it is committed to the repository. Run: git ls-files ${relpath}")
  endif()
endmacro()

# sqlite3.c uses POSIX APIs - suppress its internal warnings
set_source_files_properties(cpp/vendor/sqlite3.c 
    PROPERTIES COMPILE_FLAGS "-w")

# Link pthreads (req by SQLite on LINUX)
find_package(Threads REQUIRED)

# Check required sources so CMake fails with a helpful message instead of creating empty targets
check_required_file("cpp/src/main.cpp")
check_required_file("cpp/src/orderbook.cpp")
check_required_file("cpp/src/database.cpp")
check_required_file("cpp/src/feature_extractor.cpp")
check_required_file("cpp/src/bridge.cpp")
check_required_file("cpp/src/config.cpp")
check_required_file("cpp/src/graph.cpp")
check_required_file("tests/cpp/TEST_ORDERBOOK.cpp")
check_required_file("tests/cpp/test_database.cpp")
check_required_file("tests/cpp/test_features.cpp")
check_required_file("tests/cpp/test_config.cpp")
check_required_file("tests/cpp/test_graph.cpp")
check_required_file("tests/cpp/test_bridge.cpp")
# sqlite3.c may be vendored or downloaded by CI; do not fatal here but warn
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cpp/vendor/sqlite3.c")
  message(WARNING "cpp/vendor/sqlite3.c not found. CI may download it, or commit the amalgamation to cpp/vendor/")
endif()

# Main Engine binary
add_executable(vigil 
    cpp/src/main.cpp 
    cpp/src/orderbook.cpp
    cpp/src/database.cpp
    cpp/src/feature_extractor.cpp
    cpp/src/bridge.cpp 
    cpp/src/config.cpp 
    cpp/src/graph.cpp 
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

target_include_directories(test_orderbook PRIVATE
    cpp/include
    tests/cpp
)
target_link_libraries(test_orderbook Threads::Threads)

# Test database
add_executable(test_database
    tests/cpp/test_database.cpp
    cpp/src/database.cpp
    cpp/vendor/sqlite3.c
)

target_include_directories(test_database PRIVATE
    cpp/include
    tests/cpp
)
target_link_libraries(test_database Threads::Threads)

# Test features
add_executable(test_features
    tests/cpp/test_features.cpp 
    cpp/src/feature_extractor.cpp 
)
target_include_directories(test_features PRIVATE cpp/include tests/cpp)

# Test config
add_executable(test_config
    tests/cpp/test_config.cpp 
    cpp/src/config.cpp 
)
target_include_directories(test_config PRIVATE cpp/include tests/cpp)

# Test graph
add_executable(test_graph
    tests/cpp/test_graph.cpp 
    cpp/src/graph.cpp 
)
target_include_directories(test_graph PRIVATE cpp/include tests/cpp)

# Test bridge
add_executable(test_bridge
    tests/cpp/test_bridge.cpp
    cpp/src/bridge.cpp
)
target_include_directories(test_bridge PRIVATE cpp/include tests/cpp)

# 4. CTest integration to model
enable_testing()
add_test(NAME orderbook_tests COMMAND test_orderbook)
add_test(NAME database_tests  COMMAND test_database)
add_test(NAME feature_tests   COMMAND test_features) 
add_test(NAME config_tests    COMMAND test_config)
add_test(NAME graph_tests     COMMAND test_graph)
add_test(NAME bridge_tests COMMAND test_bridge)
