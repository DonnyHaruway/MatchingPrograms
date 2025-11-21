#include "MatchingUtils.hpp"

void generate_matchings_recursive(
    int firm_idx,
    const int &n_agents,
    const std::vector<std::vector<std::set<int>>> &candidate_map,
    std::vector<std::set<int>> &current_matching,
    std::set<int> &used_agents,
    std::vector<Matching> &result
)
{
    if (firm_idx == candidate_map.size())
    {
        Matching matching = Matching::from_firm_assignment(current_matching, n_agents);
        result.push_back(matching);
        return;
    }

    for (const auto &group : candidate_map[firm_idx])
    {
        bool valid = true;
        // agentの重複をチェック
        for (int agent : group)
        {
            if (used_agents.count(agent))
            {
                valid = false;
                break;
            }
        }
        if (!valid)
            continue;
        
        // 重複がなければ、現在のマッチングに追加して再帰呼び出し 
        for (int agent : group)
            used_agents.insert(agent);
        current_matching.push_back(group);
        generate_matchings_recursive(firm_idx + 1, n_agents, candidate_map, current_matching, used_agents, result);
        // バックトラック
        current_matching.pop_back();
        for (int agent : group)
            used_agents.erase(agent);
    }
}