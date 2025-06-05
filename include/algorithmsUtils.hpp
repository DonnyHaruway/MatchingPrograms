#pragma

#include <vector>

/// @brief 
/// @param agent 
/// @param firm 
/// @param matching 
/// @param firm_prefs 
/// @param agent_col_prefs 
/// @return 
std::pair<bool, bool> is_acceptable(int agent, int firm,
const std::vector<std::vector<int>>& matching,
const std::vector<std::vector<int>>& firm_prefs,
const std::vector<std::vector<int>>& agent_col_prefs);

/// @brief 
/// @param agent 
/// @param matching 
/// @param previous_matchings 
/// @return 
bool should_reconsider_matching(
int agent,
const std::vector<std::vector<int>>& matching,
std::map<int, std::vector<std::vector<int>>>& previous_matchings);