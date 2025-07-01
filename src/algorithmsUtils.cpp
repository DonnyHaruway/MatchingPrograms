#include "algorithmsUtils.hpp"

bool firm_acceptable(int agent, int firm,
                     const std::vector<std::set<int>> &matching,
                     const std::vector<std::vector<int>> &firm_prefs,
                     const int &firm_capacity)
{
    bool acceptable_firm;

    const int FIRM_SCORE = compute_firm_score(matching[firm], firm_prefs[firm]);

    // 企業のマッチングが満杯ならば、企業がマッチ相手を一人削除して新しいエージェントを受け入れるかどうかを確認
    if (matching[firm].size() == firm_capacity)
    {
        for (int agent_del : matching[firm])
        {
            std::set<int> matching_deleted = matching[firm];
            matching_deleted.erase(agent_del);
            matching_deleted.insert(agent);
            int tmp_score = compute_firm_score(matching_deleted, firm_prefs[firm]);
            if (tmp_score > FIRM_SCORE)
            {
                acceptable_firm = true;
                break;
            }
            else
            {
                acceptable_firm = false;
            }
        }
    }
    else
    {
        acceptable_firm = firm_prefs[firm][agent] >= 0;
    }
    return acceptable_firm;
}

bool agent_acceptable(int agent, int firm,
                      const std::vector<std::set<int>> &matching,
                      const std::vector<std::vector<int>> &agent_prefs,
                      const std::vector<std::vector<int>> &agent_col_prefs,
                      const int &firm_capacity)
{
    bool acceptable_agent = true;

    if (matching[firm].size() == firm_capacity)
    {
        std::vector<bool> agent_col_acceptable(matching[firm].size(), true);
        for (int agent_del : matching[firm])
        {
            std::set<int> matching_deleted = matching[firm];
            matching_deleted.erase(agent_del);
            matching_deleted.insert(agent);
            for (int agent_col : matching_deleted)
            {
                if (agent_col == agent)
                    continue;
                int score_before = compute_agent_score(firm, matching[firm], agent_prefs[agent_col], agent_col_prefs[agent_col]);
                int score_after = compute_agent_score(firm, matching_deleted, agent_prefs[agent_col], agent_col_prefs[agent_col]);
                if (score_after < score_before)
                {
                    agent_col_acceptable[agent_del] = false;
                    break;
                }
            }
        }
        if (std::any_of(agent_col_acceptable.begin(), agent_col_acceptable.end(), [](bool v)
                        { return !v; }))
        {
            acceptable_agent = false;
        }
    }
    else
    {
        // 企業のマッチングが満杯でない場合、同僚がエージェントを受け入れるかどうかを確認
        for (int agent_col : matching[firm])
        {
            // ここの定義は要検討
            if (agent_prefs[agent_col][firm] + agent_col_prefs[agent_col][agent] < 0)
            {
                acceptable_agent = false;
                break;
            }
        }
    }

    return acceptable_agent;
}

bool should_reconsider_matching(
    int agent,
    const std::vector<std::set<int>> &matching,
    const std::queue<int> &agent_queue,
    std::map<int, std::vector<std::pair<std::queue<int>, std::vector<std::set<int>>>>> &mp_declined)
{
    auto declined_matchings = mp_declined[agent];
    for (auto [declined_queue, declined_matching] : declined_matchings)
    {
        if (declined_queue == agent_queue && declined_matching == matching)
            return true;
    }
    return false;
}