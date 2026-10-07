#pragma once

#include "Matching.hpp"
#include "algorithmsUtils.hpp"
#include <vector>
#include <utility>
#include <queue>
#include <functional>
#include <algorithm>
#include <random>
#include <set>
#include <map>
#include <optional>

/// @brief agent をランダムな優先順で処理する dictator アルゴリズム。
/// @param seed 省略時は std::random_device から取得する (再現性が必要なら明示すること)
Matching doctor_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed = std::nullopt
);

Matching firm_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
);

Matching simple_match_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
);

Matching doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
);

/// @brief 企業が提案する DA アルゴリズム。同僚選好は考慮しない。
///        スコアは大きいほど好ましく、負のスコアは受け入れ不可。
///        企業の同順位は agent ID 順、個人の同順位は現在の企業を維持する。
Matching firm_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
);
