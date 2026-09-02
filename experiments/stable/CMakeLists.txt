cmake_minimum_required(VERSION 3.16)
project(stable_counterexample_search LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROGRAMS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(MATCHING_SOURCES
    "${PROGRAMS_ROOT}/src/MatchingSystem.cpp"
    "${PROGRAMS_ROOT}/src/Matching.cpp"
    "${PROGRAMS_ROOT}/src/Utils.cpp"
    "${PROGRAMS_ROOT}/src/MatchingUtils.cpp"
    "${PROGRAMS_ROOT}/src/algorithms.cpp"
    "${PROGRAMS_ROOT}/src/algorithmsUtils.cpp"
)

add_executable(search_stable_counterexample search_stable_counterexample.cpp ${MATCHING_SOURCES})
add_executable(search_stable_counterexample_2x3 search_stable_counterexample_2x3.cpp ${MATCHING_SOURCES})

foreach(target search_stable_counterexample search_stable_counterexample_2x3)
    target_include_directories(${target} PRIVATE "${PROGRAMS_ROOT}/include")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
