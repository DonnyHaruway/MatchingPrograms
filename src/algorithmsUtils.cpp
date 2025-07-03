#include "algorithmsUtils.hpp"
#include <iostream>

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
        acceptable_firm = firm_prefs[firm][agent] > 0;
    }
    return acceptable_firm;
}

bool agent_acceptable(int agent, int firm,
                      const std::vector<std::set<int>> &matching,
                      const std::vector<std::vector<int>> &agent_prefs,
                      const std::vector<std::vector<int>> &agent_col_prefs,
                      const int &firm_capacity)
{
    std::cout << "=== agent_acceptable ===\n";
    bool acceptable_agent = true;

    if (matching[firm].size() == firm_capacity)
    {
        std::vector<bool> combinationBool;
        for (int agent_del : matching[firm])
        {
            std::set<int> matching_deleted = matching[firm];
            matching_deleted.erase(agent_del);
            matching_deleted.insert(agent);
            std::cout << "agent_del : " << agent_del << ", agent : " << agent << '\n';
            combinationBool.push_back(can_swap_agents(firm, agent, agent_prefs, agent_col_prefs, matching[firm], matching_deleted));
        }
        if (!std::any_of(combinationBool.begin(), combinationBool.end(), [](bool v)
                        { return v; }))
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
    const std::queue<int> &agent_queue,
    const std::vector<std::set<int>> &matching,
    std::vector<std::vector<std::pair<std::queue<int>, std::vector<std::set<int>>>>> &declined)
{
    auto declined_matchings = declined[agent];
    for (auto [declined_queue, declined_matching] : declined_matchings)
    {
        if (declined_queue == agent_queue && declined_matching == matching)
            return true;
    }
    return false;
}

int exclude_one_agent(
    int agent, int firm,
    const std::vector<std::set<int>> &matching,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    const int &firm_capacitiy)
{
    int excluded_agent = -1;
    int score_diff_max = 0;
    if (matching[firm].size() == firm_capacitiy)
    {
        for (int agent_del : matching[firm])
        {
            std::set<int> matching_deleted = matching[firm];
            matching_deleted.erase(agent_del);
            matching_deleted.insert(agent);
            if (!can_swap_agents(firm, agent, agent_prefs, agent_col_prefs, matching[firm], matching_deleted)) continue;
            int score_diff_tmp = firm_prefs[firm][agent] - firm_prefs[firm][agent_del];
            for (int agent_col : matching_deleted)
            {
                if (agent_col == agent)
                    continue;
                int score_before = compute_agent_score(firm, matching[firm], agent_prefs[agent_col], agent_col_prefs[agent_col]);
                int score_after = compute_agent_score(firm, matching_deleted, agent_prefs[agent_col], agent_col_prefs[agent_col]);
                score_diff_tmp += score_after - score_before;
            }
            if (score_diff_tmp > score_diff_max)
            {
                excluded_agent = agent_del;
                score_diff_max = score_diff_tmp;
            }
        }
    }
    else
    {
        std::__throw_invalid_argument("matching[firm] must be full capacity.");
    }
    return excluded_agent;
}

bool can_swap_agents(int firm, int agent, const std::vector<std::vector<int>> &agent_prefs, const std::vector<std::vector<int>> &agent_col_prefs, const std::set<int> &matching_before, const std::set<int> &matching_after)
{   
    bool can_swap = true;
    for (int agent_col : matching_after)
    {
        if (agent_col == agent)
            continue;
        int score_before = compute_agent_score(firm, matching_before, agent_prefs[agent_col], agent_col_prefs[agent_col]);
        int score_after = compute_agent_score(firm, matching_after, agent_prefs[agent_col], agent_col_prefs[agent_col]);
        if (score_after < score_before)
        {
            can_swap = false;
            break;
        };
    }
    return can_swap;
}