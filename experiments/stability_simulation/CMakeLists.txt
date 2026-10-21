find_package(OpenMP)

set(MATCHING_SIMULATION_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/MatchingSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
)

add_library(
    stability_simulation_common
    STATIC
    simulation_common.cpp
    ${MATCHING_SIMULATION_CORE_SOURCES}
)
target_include_directories(
    stability_simulation_common
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(stability_simulation_common PUBLIC cxx_std_17)

if(OpenMP_CXX_FOUND)
    target_link_libraries(stability_simulation_common PUBLIC OpenMP::OpenMP_CXX)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(
        stability_simulation_common PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(
    simulate_matching_stability
    simulate_matching_stability.cpp
)
target_link_libraries(simulate_matching_stability PRIVATE stability_simulation_common)

add_executable(
    test_stability_simulation_common
    test_simulation_common.cpp
)
target_link_libraries(
    test_stability_simulation_common PRIVATE stability_simulation_common)
add_test(
    NAME stability_simulation_common
    COMMAND test_stability_simulation_common
)

add_library(iterated_da_comparison STATIC iterated_da_comparison.cpp)
target_link_libraries(iterated_da_comparison PUBLIC stability_simulation_common)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(iterated_da_comparison PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(compare_iterated_da_5x8 compare_iterated_da_5x8.cpp)
target_link_libraries(compare_iterated_da_5x8 PRIVATE iterated_da_comparison)

add_executable(exhaustive_iterated_da_profile exhaustive_iterated_da_profile.cpp)
target_link_libraries(exhaustive_iterated_da_profile PRIVATE iterated_da_comparison)

add_executable(test_iterated_da_comparison test_iterated_da_comparison.cpp)
target_link_libraries(test_iterated_da_comparison PRIVATE iterated_da_comparison gtest_main)
gtest_discover_tests(test_iterated_da_comparison DISCOVERY_TIMEOUT 60)
