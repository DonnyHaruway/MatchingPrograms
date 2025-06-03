#pragma once

#include "Matching.hpp"
#include "algorithmsUtils.hpp"
#include <vector>
#include <utility>

/// @brief return the matching result of serial dictator-like algorithm
/// @return matching result

Matching run_dictator_like_algorithm(
    const int& n_agents,
    const int& n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
);