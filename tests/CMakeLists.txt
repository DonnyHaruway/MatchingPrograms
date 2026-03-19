# tests/CMakeLists.txt

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

enable_testing()

find_package(OpenMP)

# includeディレクトリを設定
include_directories(${CMAKE_SOURCE_DIR}/include)

# テストディレクトリの一覧
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp")

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(
        ${TEST_NAME}
        ${TEST_SRC}
        ${CMAKE_SOURCE_DIR}/src/MatchingSystem.cpp
        ${CMAKE_SOURCE_DIR}/src/Matching.cpp
        ${CMAKE_SOURCE_DIR}/src/Utils.cpp
        ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
        ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
        ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
    )

    target_link_libraries(${TEST_NAME} gtest_main)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${TEST_NAME} OpenMP::OpenMP_CXX)
    endif()
    gtest_discover_tests(${TEST_NAME})
endforeach()