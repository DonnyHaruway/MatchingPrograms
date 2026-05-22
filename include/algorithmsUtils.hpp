#pragma once

#include <vector>
#include <set>
#include <map>
#include <queue>
#include "Utils.hpp"
#include "CommonTypes.hpp"

using MatchingTypes::FirmMatching;


/// @brief 現状の各企業のマッチングの、1人以上容量に余裕がある部分集合の配列を返す
/// @param firm_match 
/// @param firm_capacities 
/// @return (企業, 個人集合)のpairの配列
std::vector<FirmMatching> create_current_firm_match_subsets(
    const std::vector<std::set<int>> &firm_match, 
    const std::vector<int> &firm_capacities
);

/// @brief 現在のマッチングでagentのみが自由に動ける時に最もそのagentにとって望ましいmatchingを求める。
/// @param agent 
/// @param agent_prefs 
/// @param agent_col_prefs 
/// @param all_firm_match 
/// @param unofferable 
/// @return FirmMatching
FirmMatching find_prefered_match (
    const int &agent, 
    const std::vector<int> &agent_pref, 
    const std::vector<int> &agent_col_pref, 
    const std::vector<FirmMatching> &all_firm_match,
    const std::set<FirmMatching> &unofferable
);

bool is_firm_match_accept_propose(
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

bool agents_accept_match(
    const std::set<int> &candidate_set,
    const int &firm,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
);

/// @brief 指定したfirmについてbest type1 blocking pairのagentを返す。存在しなければ-1
/// @param firm 対象のfirm
/// @param firm_match 現在の各firmのマッチング
/// @param agent_match 現在の各agentのマッチング
/// @param agent_prefs
/// @param firm_prefs
/// @param agent_col_prefs
/// @param firm_capacities
/// @return best type1 blocking pairのagent番号、存在しなければ-1
int find_best_type1_blocking_pair(
    const int &firm,
    const std::vector<std::set<int>> &firm_match,
    const std::vector<int> &agent_match,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    const std::vector<int> &firm_capacities
);

/// @brief type1 blocking pair(for each firms, if not found match -1)
/// @param firm_match 
/// @param agent_match 
/// @param agent_prefs 
/// @param firm_prefs 
/// @param agent_col_prefs 
/// @param firm_capacities 
/// @return 
std::vector<int> find_best_type1_blocking_pair_list(
    const std::vector<std::set<int>> &firm_match,
    const std::vector<int> &agent_match,
    const std::vector<std::vector<int>> &agent_prefs, 
    const std::vector<std::vector<int>> &firm_prefs, 
    const std::vector<std::vector<int>> &agent_col_prefs,
    const std::vector<int> &firm_capacities
);