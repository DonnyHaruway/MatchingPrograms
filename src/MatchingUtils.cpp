#include "MatchingUtils.hpp"

void generate_matchings_recursive(
    int firm_idx,
    const int &n_agents,
    const std::vector<std::vector<std::set<int>>> &all_candidates,
    std::vector<std::set<int>> &current_matching,
    std::set<int> &used_agents,
    std::vector<Matching> &result)
{
    // ベースケース：すべての企業に割り当て終わったら結果に追加
    if (firm_idx == all_candidates.size())
    {
        Matching matching = Matching::from_firm_assignment(current_matching, n_agents);
        result.push_back(matching);
        return;
    }

    // 候補をすべて試す
    for (const auto &group : all_candidates[firm_idx])
    {
        bool valid = true;

        // 重複agentチェック
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

        // 割り当てを追加
        for (int agent : group)
            used_agents.insert(agent);
        current_matching.push_back(group);

        // 再帰的に次の企業へ
        generate_matchings_recursive(firm_idx + 1, n_agents, all_candidates, current_matching, used_agents, result);

        // バックトラック
        current_matching.pop_back();
        for (int agent : group)
            used_agents.erase(agent);
    }
}