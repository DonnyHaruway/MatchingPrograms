#include "algorithms.hpp"

Matching run_dictator_like_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs)
{
    // step1: agentをランダムに並べてqueueに格納
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(agent_ids.begin(), agent_ids.end(), rng);

    std::queue<int> agent_queue;
    for (int id : agent_ids)
        agent_queue.push(id);

    // step2: agentのマッチング, firmのマッチング, 各firm, agentのスコアの初期化
    std::vector<int> agent_matching(n_agents, -1);
    std::vector<std::set<int>> firm_matching(n_firms);
    std::vector<int> firm_scores(n_firms, 0);
    std::vector<int> agent_scores(n_agents, 0);

    // 同僚が原因でエージェントのマッチが決まらなかった時にその状態を記録する
    // agent : 個人のqueue、各企業とそのマッチ相手のvectorの集合のpair
    std::map<int, std::vector<std::pair<std::queue<int>, std::vector<std::set<int>>>>> mp_declined;

    // 告白できない企業のリスト
    // agent : 告白できない企業set
    std::vector<std::set<int>> unofferable(n_agents);

    // step3: queueの先頭のエージェントが最も好む集合にマッチさせる
    while (agent_queue.size())
    {
        int agent = agent_queue.front();
        agent_queue.pop();
        int prefered_firm = -1;
        std::vector<bool> firm_accept(n_firms);
        std::vector<bool> agent_accept(n_firms);
        // 全てのマッチ先のスコアを検索
        for (int firm = 0; firm < n_firms; firm++)
        {
            if (unofferable[agent].count(firm)) continue;
            int agent_score_tmp = 0;

            bool acceptable_firm = firm_acceptable(agent, firm, firm_matching, firm_prefs, firm_capacities[firm]);
            bool acceptable_agent = agent_acceptable(agent, firm, firm_matching, agent_prefs, agent_col_prefs, firm_capacities[firm]);

            firm_accept[firm] = acceptable_firm;
            agent_accept[firm] = acceptable_agent;

            if (acceptable_firm && acceptable_agent)
            {
                agent_score_tmp += agent_prefs[agent][firm];
                for (int agent_col : firm_matching[firm]) agent_score_tmp += agent_col_prefs[agent][agent_col];
                if (agent_score_tmp > agent_scores[agent]) {
                    prefered_firm = firm;
                    agent_scores[agent] = agent_score_tmp;
                }
            } else if (!acceptable_firm) {
                unofferable[agent].insert(firm);
            }
        }
        
        if (prefered_firm == -1) {
            if (!std::any_of(firm_accept.begin(), firm_accept.end(), [] (bool v) { return !v; })) {
                mp_declined[agent].emplace_back(agent_queue, firm_matching);
                continue;
            } else {
                continue;
            }
        }

        firm_matching[prefered_firm].insert(agent);
        agent_matching[agent] = prefered_firm;
    }
    return Matching::from_firm_assignment(firm_matching, n_agents);
};