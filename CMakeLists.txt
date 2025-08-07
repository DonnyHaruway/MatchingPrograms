cmake_minimum_required(VERSION 3.10)
project(MatchingSim)

set(CMAKE_CXX_STANDARD 17)           # Set C++ standard to C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # C++17を必須にする

include_directories(include)    # ヘッダファイルの場所

# 実行ファイルを作るためのソースリスト
# add_executable(matching_sim
#     src/MatchingSystem.cpp
#     src/Matching.cpp
#     src/Utils.cpp
#     src/MatchingUtils.cpp
#     src/algorithms.cpp
#     src/algorithmsUtils.cpp
# )

# テストフォルダもビルドに含める
enable_testing()
add_subdirectory(tests)