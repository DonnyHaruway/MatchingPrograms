#include "algorithms.hpp"
#include "CommonTypes.hpp"

using namespace MatchingTypes;


Matching run_dictator_like_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    // agentをランダムに並べてqueueに格納
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(agent_ids.begin(), agent_ids.end(), rng);
    std::queue<int> agent_queue;
    for (int id : agent_ids)
        agent_queue.push(id);

    std::vector<std::set<int>> firm_matching(n_firms);

    // 告白リスト
    // agent : (企業, 個人の集合)
    std::vector<std::set<FirmMatching>> unofferable_list(n_agents);

    // queueが空になるまでqueueの先頭のエージェントが最も好む集合にマッチさせる
    while (!agent_queue.empty())
    {
        int agent = agent_queue.front();
        agent_queue.pop();
        std::vector<FirmMatching> all_firm_matching = create_all_firm_matching(firm_matching, firm_capacities);
        FirmMatching prefered_match = find_prefered_match(agent, agent_prefs[agent], agent_col_prefs[agent], all_firm_matching, unofferable_list[agent]);
        // もう告白したい集合がない
        if (prefered_match.first == -1) continue;

        // 告白先が受け入れ可能な場合
        if (firm_match_accept_propose(agent, prefered_match, firm_matching[prefered_match.first], agent_prefs, firm_prefs, agent_col_prefs)) {
            // 削除されたエージェントがいればqueueに追加する
            std::set<int> firm_matching_before = firm_matching[prefered_match.first];
            std::set<int> firm_matching_after = prefered_match.second;
            int agent_deleted = find_agent_deleted(firm_matching_before, firm_matching_after);
            if (agent_deleted != -1) agent_queue.push(agent_deleted);

            // 告白先の集合を更新
            prefered_match.second.insert(agent);
            firm_matching[prefered_match.first] = prefered_match.second;
        } else {
            unofferable_list[agent].emplace(prefered_match);
            agent_queue.push(agent);
        }
    }
    return Matching::from_firm_assignment(firm_matching, n_agents);
};

Matching run_doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
)
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