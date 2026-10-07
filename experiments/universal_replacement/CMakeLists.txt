add_executable(
    exhaustive_universal_replacement_5x8
    exhaustive_5x8.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
)
target_include_directories(
    exhaustive_universal_replacement_5x8 PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(exhaustive_universal_replacement_5x8 PRIVATE cxx_std_17)
# This is a single-profile search. Do not enable per-matching OpenMP regions.

add_executable(
    exhaustive_da_paths_5x8
    exhaustive_da_paths.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
)
target_include_directories(exhaustive_da_paths_5x8 PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(exhaustive_da_paths_5x8 PRIVATE cxx_std_17)
