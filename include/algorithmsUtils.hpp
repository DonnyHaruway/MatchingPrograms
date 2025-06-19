#pragma

#include <vector>
#include <set>
#include <map>
#include <queue>

/// @brief 
/// @param agent 
/// @param firm 
/// @param matching 
/// @param firm_prefs 
/// @param agent_col_prefs 
/// @return 
std::pair<bool, bool> is_acceptable(int agent, int firm,
const std::vector<std::set<int>>& matching,
const std::vector<std::vector<int>>& firm_prefs,
const std::vector<std::vector<int>>& agent_col_prefs);

/// @brief 
/// @param agent 
/// @param matching 
/// @param matching_declined
/// @return 
bool should_reconsider_matching(
int agent,
const std::vector<std::set<int>>& matching,
std::map<int, std::vector<std::pair<std::queue<int>,std::vector<std::set<int>>>>>& mp_declined);