find_package(OpenMP)

add_executable(
    exhaustive_weakly_stable_binary_col_prefs
    ${CMAKE_CURRENT_SOURCE_DIR}/weakly_stable_binary_col_prefs.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Matching.cpp
    ${CMAKE_SOURCE_DIR}/src/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/MatchingUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithms.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithmsUtils.cpp
)

target_include_directories(exhaustive_weakly_stable_binary_col_prefs PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(OpenMP_CXX_FOUND)
    target_link_libraries(exhaustive_weakly_stable_binary_col_prefs OpenMP::OpenMP_CXX)
endif()
