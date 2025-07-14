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
    std::cout << "agent_ids\n";
    for (int x : agent_ids)
        std::cout << x << ' ';
    std::cout << '\n';
    std::queue<int> agent_queue;
    for (int id : agent_ids)
        agent_queue.push(id);

    // step2: agentのマッチング, firmのマッチング, 各firm, agentのスコアの初期化
    std::vector<std::set<int>> firm_matching(n_firms);
    std::vector<int> firm_scores(n_firms, 0);
    std::vector<int> agent_scores(n_agents, 0);

    // 同僚が原因でエージェントのマッチが決まらなかった時にその状態を記録する
    // agent : 個人のqueue、各企業とそのマッチ相手のvectorの集合のpair
    std::vector<std::vector<std::pair<std::queue<int>, std::vector<std::set<int>>>>> declined(n_agents);

    // 告白できない企業のリスト
    // agent : 告白できない企業set
    std::vector<std::set<int>> unofferable(n_agents);

    // step3: queueの先頭のエージェントが最も好む集合にマッチさせる
    while (agent_queue.size())
    {
        std::cout << "=== agent_queue at start of loop ===\n";
        std::queue<int> tmp_q = agent_queue;
        while (!tmp_q.empty())
        {
            std::cout << tmp_q.front() << ' ';
            tmp_q.pop();
        }
        std::cout << '\n';
        int agent = agent_queue.front();
        agent_queue.pop();
        if (should_reconsider_matching(agent, agent_queue, firm_matching, declined))
            continue;

        int prefered_firm = -1;
        std::vector<bool> firm_accept(n_firms);
        std::vector<bool> agent_accept(n_firms);

        // 全てのマッチ先のスコアを検索
        for (int firm = 0; firm < n_firms; firm++)
        {

            if (unofferable[agent].count(firm))
                continue;

            int agent_score_tmp = 0;
            bool acceptable_firm = firm_acceptable(agent, firm, firm_matching, firm_prefs, firm_capacities[firm]);
            bool acceptable_agent = agent_acceptable(agent, firm, firm_matching, agent_prefs, agent_col_prefs, firm_capacities[firm]);

            firm_accept[firm] = acceptable_firm;
            agent_accept[firm] = acceptable_agent;

            if (acceptable_firm && acceptable_agent)
            {
                agent_score_tmp += agent_prefs[agent][firm];
                for (int agent_col : firm_matching[firm])
                    agent_score_tmp += agent_col_prefs[agent][agent_col];
                if (agent_score_tmp > agent_scores[agent])
                {
                    prefered_firm = firm;
                    agent_scores[agent] = agent_score_tmp;
                }
            }
            else if (!acceptable_firm)
            {
                unofferable[agent].insert(firm);
            }
        }
        if (prefered_firm == -1)
        {
            if (!std::any_of(agent_accept.begin(), agent_accept.end(), [](bool v)
                             { return v; }))
            {
                declined[agent].emplace_back(agent_queue, firm_matching);
                agent_queue.push(agent);
                continue;
            }
            else
            {
                continue;
            }
        }
        if (firm_matching[prefered_firm].size() == firm_capacities[prefered_firm])
        {
            int excluded_agent = exclude_one_agent(agent, prefered_firm, firm_matching, agent_prefs, firm_prefs, agent_col_prefs, firm_capacities[prefered_firm]);
            firm_matching[prefered_firm].erase(excluded_agent);
            firm_matching[prefered_firm].insert(agent);
        }
        else
        {
            firm_matching[prefered_firm].insert(agent);
        }
    }
    return Matching::from_firm_assignment(firm_matching, n_agents);
};

Matching run_doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs)
{
    std::vector<std::set<int>> firm_matching(n_firms);
    std::vector<bool> is_matched(n_agents);
    std::vector<std::vector<bool>> confess_lists(n_agents, std::vector<bool>(n_firms));
    while (true) {
        std::vector<int> confess(n_agents);
        for (int agent=0; agent<n_agents; agent++) {
            if (is_matched[agent]) continue;
            int target_firm = -1;
            int max_score = -1;
            for (int firm=0; firm<n_agents; firm++) {
                if (agent_prefs[agent][firm] > max_score && !confess_lists[agent][firm]) {
                    max_score = agent_prefs[agent][firm];
                    target_firm = firm;
                }
            }
            confess[agent] = target_firm;
        }
        
        for (int agent=0; agent<n_agents; agent++) {
            if (is_matched[agent]) continue;
            int target_firm = confess[agent];
            if (firm_prefs[target_firm][agent] >= 0 && firm_matching[target_firm].size() < firm_capacities[target_firm]) {
                firm_matching[target_firm].insert(agent);
                is_matched[agent] = true;
            } else {
                confess_lists[agent][target_firm] = true;
            }
        }
        if (std::all_of(is_matched.begin(), is_matched.end(), [](bool b) { return b; })) break;
        if (all_rejected(is_matched, confess_lists)) break;
    }

    return Matching::from_firm_assignment(firm_matching, n_agents);
}