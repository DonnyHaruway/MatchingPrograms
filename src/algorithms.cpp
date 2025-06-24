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
        int agent = agent_queue.front(); agent_queue.pop();
        int prefered_firm = -1;
        int agent_score_tmp = agent_scores[agent];

        // 全てのマッチ先のスコアを検索
        for (int firm = 0; firm < n_firms; firm++)
        {
            // 告白可能ならば計算
            if (!unofferable[agent].count(firm))
            {
                agent_score_tmp += agent_prefs[agent][firm];
                // 現在のmatchingにおけるキャパシティを比較
                if (firm_matching[firm].size() == firm_capacities[firm])
                {
                    
                }
                else
                {
                    for (int agent_col : firm_matching[firm])
                        agent_score_tmp += agent_col_prefs[agent][agent_col];
                }
                // match相手を決定
                prefered_firm = agent_scores[agent] < agent_score_tmp ? firm : prefered_firm;
            }
        }
        if (prefered_firm == -1)
            continue;

        // 受け入れ可能か確認
        bool acceptable_firm = firm_acceptable(agent, prefered_firm, firm_matching, firm_prefs, firm_capacities);
        // 受け入れ可能なら追加
        if (acceptable_firm && acceptable_agent) {
                firm_matching[prefered_firm].insert(agent);
                agent_matching[agent] = prefered_firm;
            }
        // 同僚が原因で弾かれた場合
        else if (acceptable_firm)
        {
            // 過去に弾かれた状況と今が同じかどうかを判定
            bool same = should_reconsider_matching(agent, agent_queue, firm_matching, mp_declined);
            // 過去の状況と異なればqueueに追加
            if (!same)
            {
                agent_queue.push(agent);
                mp_declined[agent].push_back(std::pair(agent_queue, firm_matching));
            }
        }
        // 企業側に弾かれた場合
        else
        {
            unofferable[agent].insert(prefered_firm);
            agent_queue.push(agent);
        }
    }
    return Matching::from_firm_assignment(firm_matching, n_agents);
};