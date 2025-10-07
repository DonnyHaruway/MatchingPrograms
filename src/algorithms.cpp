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

    std::vector<std::set<int>> firm_match(n_firms);

    // 告白リスト
    // agent : (企業, 個人の集合)
    std::vector<std::set<FirmMatching>> unofferable_list(n_agents);

    // queueが空になるまでqueueの先頭のエージェントが最も好む集合にマッチさせる
    while (!agent_queue.empty())
    {   
        int agent = agent_queue.front();
        agent_queue.pop();

        std::vector<FirmMatching> all_firm_match = create_all_firm_match(firm_match, firm_capacities);
        FirmMatching prefered_match = find_prefered_match(agent, agent_prefs[agent], agent_col_prefs[agent], all_firm_match, unofferable_list[agent]);
        // もう告白したい集合がない
        if (prefered_match.first == -1) {
            continue;
        }
        // 告白先が受け入れ可能な場合
        if (firm_match_accept_propose(agent, prefered_match, firm_match[prefered_match.first], agent_prefs, firm_prefs, agent_col_prefs)) {
            // 削除されたエージェントがいればqueueに追加する
            
            std::set<int> firm_match_before = firm_match[prefered_match.first];
            std::set<int> firm_match_after = prefered_match.second;
            prefered_match.second.insert(agent);
            int agent_deleted = find_agent_deleted(firm_match_before, firm_match_after);
            if (agent_deleted != -1) {
                agent_queue.push(agent_deleted);
                unofferable_list[agent_deleted].emplace(prefered_match.first, firm_match_after);
            } 
            firm_match[prefered_match.first] = prefered_match.second;
        } else {
            unofferable_list[agent].emplace(prefered_match);
            agent_queue.push(agent);
        }
    }
    return Matching::from_firm_assignment(firm_match, n_agents);
};

Matching run_doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
)
{
    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<std::vector<int>> agent_order(n_agents);
    agent_order.assign(n_agents, {});
    // std::cout << "[doctor_proposing_DA] START" << std::endl;
    for (int a = 0; a < n_agents; ++a) {
        std::vector<int> firms(n_firms);
        std::iota(firms.begin(), firms.end(), 0);
        std::sort(firms.begin(), firms.end(), [&](int f1, int f2){
            return agent_prefs[a][f1] > agent_prefs[a][f2];
        });
        agent_order[a] = firms;
    }
    // std::cout << "[doctor_proposing_DA] agent_order done" << std::endl;
    std::vector<int> next_idx(n_agents, 0);
    std::queue<int> free_agents;
    // std::cout << "[doctor_proposing_DA] queue init done" << std::endl;
    for (int a = 0; a < n_agents; ++a) free_agents.push(a);
    // std::cout << "[doctor_proposing_DA] queue fill done" << std::endl;
    auto worst_in = [&](int firm) -> int {
        int worst = -1;
        int worst_score = -1e9; // とりあえず
        for (int a : firm_match[firm]) {
            int sc = firm_prefs[firm][a];
            if (sc < worst_score) { worst_score = sc; worst = a; }
        }
        return worst;
    };
    while (!free_agents.empty()) {
        int agent = free_agents.front();
        free_agents.pop();
        if (next_idx[agent] >= n_firms) continue;
        int firm = agent_order[agent][next_idx[agent]];
        next_idx[agent]++;
        // firmの空きがある場合
        if (firm_match[firm].size() < firm_capacities[firm] && firm_prefs[firm][agent] >= 0) {
            firm_match[firm].insert(agent);
        } else {
            int worst_agent = worst_in(firm);
            if (worst_agent == -1) continue;
            if (firm_prefs[firm][agent] > firm_prefs[firm][worst_agent]) {
                firm_match[firm].erase(worst_agent);
                firm_match[firm].insert(agent);
                free_agents.push(worst_agent);
            } else {
                free_agents.push(agent);
            }
        }
    }
    // std::cout << "[doctor_proposing_DA] firm_match result" << std::endl;
    // for (int f = 0; f < n_firms; f++) {
    //     std::cout << "  Firm " << f << " : ";
    //     for (int a : firm_match[f]) std::cout << a << " ";
    //     std::cout << std::endl;
    // }
    // std::cout << "[doctor_proposing_DA] END" << std::endl;
    return Matching::from_firm_assignment(firm_match, n_agents);
}