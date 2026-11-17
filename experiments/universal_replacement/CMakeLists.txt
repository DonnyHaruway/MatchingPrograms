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

add_executable(
    da_search_trees
    da_search_trees.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
)
target_include_directories(da_search_trees PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(da_search_trees PRIVATE cxx_std_17)

add_executable(
    run_rda_batch
    run_rda_batch.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
)
target_include_directories(run_rda_batch PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(run_rda_batch PRIVATE cxx_std_17)

add_executable(
    rda_stability_engine
    rda_stability_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
)
target_include_directories(rda_stability_engine PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(rda_stability_engine PRIVATE cxx_std_17)

add_executable(
    run_rda_exhaustive
    run_rda_exhaustive.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
)
target_include_directories(run_rda_exhaustive PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(run_rda_exhaustive PRIVATE cxx_std_17)
add_dependencies(run_rda_exhaustive rda_stability_engine)
target_compile_definitions(run_rda_exhaustive PRIVATE
    RDA_STABILITY_DRIVER="${CMAKE_CURRENT_SOURCE_DIR}/run_rda_stability_exhaustive.py"
    RDA_STABILITY_ENGINE="$<TARGET_FILE:rda_stability_engine>")
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME RDAThreeStabilitiesCLI
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/algorithms/test_rda_three_stabilities_cli.py"
            "$<TARGET_FILE:run_rda_exhaustive>")
endif()
# Distribute disjoint input-ID ranges across processes; avoid tiny per-DA OpenMP regions.
