cmake_minimum_required(VERSION 3.10)
project(MatchingSim)

set(CMAKE_CXX_STANDARD 17)           # Set C++ standard to C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # C++17を必須にする

include_directories(include)

# テストフォルダもビルドに含める
enable_testing()
add_subdirectory(tests)
add_subdirectory(experiments/exhaustive)