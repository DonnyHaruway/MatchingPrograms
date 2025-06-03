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

# test_manual_add.cppをコンパイル
add_executable(test_manual_add
    test_manual_add.cpp
    ../src/MatchingSystem.cpp
    ../src/Utils.cpp
)

target_include_directories(test_manual_add PUBLIC ../include)
target_link_libraries(test_manual_add gtest_main)

# test_evaluate.cppをコンパイル
add_executable(test_evaluate
    test_evaluate.cpp
    ../src/MatchingSystem.cpp
    ../src/Matching.cpp
    ../src/Utils.cpp
    ../src/algorithms.cpp        
    ../src/algorithmsUtils.cpp 
)

target_include_directories(test_evaluate PUBLIC ../include)
target_link_libraries(test_evaluate gtest_main)

# test_matching.cppをコンパイル
add_executable(test_matching
    test_matching.cpp
    ../src/Matching.cpp
    ../src/Utils.cpp
)

target_include_directories(test_matching PUBLIC ../include)
target_link_libraries(test_matching gtest_main)

# gtestによるテスト検出
include(GoogleTest)
gtest_discover_tests(test_generate)
gtest_discover_tests(test_manual_add)
gtest_discover_tests(test_evaluate)
gtest_discover_tests(test_matching)