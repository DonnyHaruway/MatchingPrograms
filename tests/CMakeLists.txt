include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)

# テスト実行ファイル(test_generate.cppをコンパイル)
add_executable(test_generate
    test_generate.cpp
    ../src/MatchingSystem.cpp
    ../src/Utils.cpp
)

# ヘッダファイルの場所を指定
target_include_directories(test_generate PUBLIC ../include)

# GoogleTest ライブラリにリンク
target_link_libraries(test_generate gtest_main)

# gtestによるテスト検出
include(GoogleTest)
gtest_discover_tests(test_generate)