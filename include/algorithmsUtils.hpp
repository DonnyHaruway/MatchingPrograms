#pragma once

#include <vector>
#include <set>
#include <map>
#include <queue>
#include "Utils.hpp"
#include "CommonTypes.hpp"

using MatchingTypes::FirmMatching;

std::vector<FirmMatching> create_all_firm_match (
    const std::vector<std::set<int>> &firm_match, 
    const std::vector<int> &firm_capacities
);

/// @brief agent自身は評価対象のマッチングに含まれていない
/// @param agent 
/// @param agent_prefs 
/// @param agent_col_prefs 
/// @param all_firm_match 
/// @param unofferable 
/// @return 
FirmMatching find_prefered_match (
    const int &agent, 
    const std::vector<int> &agent_pref, 
    const std::vector<int> &agent_col_pref, 
    const std::vector<FirmMatching> &all_firm_match,
    const std::set<FirmMatching> &unofferable
);

bool firm_match_accept_propose(
    const int &agent, 
    const FirmMatching &prefered_match, 
    const std::set<int> &firm_match_before, 
    const std::vector<std::vector<int>> &agent_prefs, 
    const std::vector<std::vector<int>> &firm_prefs, 
    const std::vector<std::vector<int>> &agent_col_prefs
);

std::set<int> find_agent_deleted(
    const std::set<int> &firm_match_before, 
    const std::set<int> &firm_match_after
);

int exclude_one_agent (
    int agent, 
    int firm,
    const std::vector<std::set<int>> &matching,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    const int &firm_capacitiy
);

bool can_swap_agents (
    int firm, 
    int agent, 
    const std::vector<std::vector<int>> &agent_prefs, 
    const std::vector<std::vector<int>> &agent_col_prefs, 
    const std::set<int> &matching_before, 
    const std::set<int> &matching_after
);

bool all_rejected (
    const std::vector<bool> &is_matched, 
    const std::vector<std::vector<bool>> &confess_lists
);