include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)

# テスト実行ファイル(test_ms_generate.cppをコンパイル)
add_executable(test_ms_generate
    test_ms_generate.cpp
    ../src/MatchingSystem.cpp
    ../src/Utils.cpp
)

# ヘッダファイルの場所を指定
target_include_directories(test_ms_generate PUBLIC ../include)

# GoogleTest ライブラリにリンク
target_link_libraries(test_ms_generate gtest_main)

# test_ms_manual_add.cppをコンパイル
add_executable(test_ms_manual_add
    test_ms_manual_add.cpp
    ../src/MatchingSystem.cpp
    ../src/Utils.cpp
)

target_include_directories(test_ms_manual_add PUBLIC ../include)
target_link_libraries(test_ms_manual_add gtest_main)

# test_ms_evaluate.cppをコンパイル
add_executable(test_ms_evaluate
    test_ms_evaluate.cpp
    ../src/MatchingSystem.cpp
    ../src/Matching.cpp
    ../src/Utils.cpp
    ../src/algorithms.cpp        
    ../src/algorithmsUtils.cpp 
)

target_include_directories(test_ms_evaluate PUBLIC ../include)
target_link_libraries(test_ms_evaluate gtest_main)

# test_matching.cppをコンパイル
add_executable(test_matching
    test_matching.cpp
    ../src/Matching.cpp
    ../src/Utils.cpp
)

target_include_directories(test_matching PUBLIC ../include)
target_link_libraries(test_matching gtest_main)

# test_utils.cppをコンパイル
add_executable(test_utils
    test_utils.cpp
    ../src/Utils.cpp
    ../src/Matching.cpp
)

target_include_directories(test_utils PUBLIC ../include)
target_link_libraries(test_utils gtest_main)

# test_alg_dictator.cppをコンパイル
add_executable(test_alg_dictator
    test_alg_dictator.cpp
    ../src/MatchingSystem.cpp
    ../src/Matching.cpp
    ../src/Utils.cpp
    ../src/algorithms.cpp        
    ../src/algorithmsUtils.cpp 
)

target_include_directories(test_alg_dictator PUBLIC ../include)
target_link_libraries(test_alg_dictator gtest_main)

# gtestによるテスト検出
include(GoogleTest)
gtest_discover_tests(test_ms_generate)
gtest_discover_tests(test_ms_manual_add)
gtest_discover_tests(test_ms_evaluate)
gtest_discover_tests(test_matching)
gtest_discover_tests(test_utils)
gtest_discover_tests(test_alg_dictator)