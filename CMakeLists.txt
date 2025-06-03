cmake_minimum_required(VERSION 3.10)
project(MatchingSim)

set(CMAKE_CXX_STANDARD 17)           # Set C++ standard to C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # C++17を必須にする

include_directories(include)    # ヘッダファイルの場所

# 実行ファイルを作るためのソースリスト
add_executable(matching_sim
    main.cpp
    src/MatchingSystem.cpp
    src/Matching.cpp
    src/Utils.cpp
)

# (あとで追加)テストフォルダもビルドに含めたいので準備
enable_testing()
add_subdirectory(tests)