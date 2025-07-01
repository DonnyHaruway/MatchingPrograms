#pragma

#include <vector>
#include <set>
#include <map>
#include <queue>
#include <Utils.hpp>

/// @brief 指定したfirmがagentを受け入れ可能かどうかを表す
/// @param agent
/// @param firm
/// @param matching
/// @param firm_prefs
/// @param agent_col_prefs
/// @return
bool firm_acceptable(int agent, int firm,
                     const std::vector<std::set<int>> &matching,
                     const std::vector<std::vector<int>> &firm_prefs,
                     const int &firm_capacity);

bool agent_acceptable(int agent, int firm,
                      const std::vector<std::set<int>> &matching,
                      const std::vector<std::vector<int>> &agent_prefs,
                      const std::vector<std::vector<int>> &agent_col_prefs,
                      const int &firm_capacitiy);

/// @brief
/// @param agent
/// @param matching
/// @param matching_declined
/// @return
bool should_reconsider_matching(
    int agent,
    const std::queue<int> &agent_queue,
    const std::vector<std::set<int>> &matching,
    std::map<int, std::vector<std::pair<std::queue<int>, std::vector<std::set<int>>>>> &mp_declined);
