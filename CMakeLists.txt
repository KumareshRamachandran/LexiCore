cmake_minimum_required(VERSION 3.20)
project(LexiCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library target (all core modules)
add_library(lexicore_lib
    src/dictionary.cpp
    src/edit_distance.cpp
    src/trie.cpp
    src/bktree.cpp
    src/ranking.cpp
    src/benchmark.cpp
)
target_include_directories(lexicore_lib PUBLIC include)

# Main executable
add_executable(lexicore app/main.cpp)
target_link_libraries(lexicore PRIVATE lexicore_lib)

# Test executables
enable_testing()
foreach(test_name test_edit_distance test_trie test_bktree test_correctness)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE lexicore_lib)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
