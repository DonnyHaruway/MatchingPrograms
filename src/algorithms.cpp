#include "algorithms.hpp"
#include "CommonTypes.hpp"

using namespace MatchingTypes;


Matching doctor_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);
    // std::random_device rd;
    // std::mt19937 rng(rd());
    // std::shuffle(agent_ids.begin(), agent_ids.end(), rng);
    std::queue<int> agent_queue;
    for (int id : agent_ids) {
        agent_queue.push(id);
    }

    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<std::set<FirmMatching>> unofferable_list(n_agents);

    while (!agent_queue.empty())
    {   
        std::queue<int> tmp_q = agent_queue;

        int agent = agent_queue.front();
        agent_queue.pop();

        std::vector<FirmMatching> current_firm_match_subsets = create_current_firm_match_subsets(firm_match, firm_capacities);
        FirmMatching prefered_match = find_prefered_match(agent, agent_prefs[agent], agent_col_prefs[agent], current_firm_match_subsets, unofferable_list[agent]);
        if (prefered_match.first == -1) {
            continue;
        }
        unofferable_list[agent].emplace(prefered_match);
        if (is_firm_match_accept_propose(agent, prefered_match, firm_match[prefered_match.first], agent_prefs, firm_prefs, agent_col_prefs)) {
            std::set<int> firm_match_before = firm_match[prefered_match.first];
            std::set<int> firm_match_after = prefered_match.second;
            prefered_match.second.insert(agent);
            std::set<int> agent_deleted = find_agent_deleted(firm_match_before, firm_match_after);
                for (int a : agent_deleted) {
                    agent_queue.push(a);
                    unofferable_list[a].emplace(prefered_match.first, firm_match_after);
                }
            firm_match[prefered_match.first] = prefered_match.second;
        } else {
            agent_queue.push(agent);
        }
    }
    return Matching::from_firm_assignment(firm_match, n_agents);
};

Matching firm_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    std::vector<int> firm_ids(n_firms);
    std::iota(firm_ids.begin(), firm_ids.end(), 0);
    std::queue<int> firm_queue;
    for (int id : firm_ids) {
        firm_queue.push(id);
    }
    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<int> single_agents(n_agents);
    std::iota(single_agents.begin(), single_agents.end(), 0);
    while (!firm_queue.empty())
    {   
        int firm = firm_queue.front();
        firm_queue.pop();

        std::set<int> match = {};
        int current_score = 0;
        auto candidates_map = generate_all_subsets_by_size(single_agents, firm_capacities[firm]);
        for (int size = firm_capacities[firm]; size >= 0; size--) {
            for (auto candidate_set : candidates_map[size]) {
                // Check if all agents in candidate_set would accept the proposal
                if (!agents_accept_match(candidate_set, firm, agent_prefs, firm_prefs, agent_col_prefs)) continue;
                int firm_tmp_score = compute_firm_score(candidate_set, firm_prefs[firm]);
                if (firm_tmp_score < current_score) continue;
                match = candidate_set;
                current_score = firm_tmp_score;
            }
        }
        firm_match[firm] = match;
        for (int agent : match) {
            single_agents.erase(std::remove(single_agents.begin(), single_agents.end(), agent), single_agents.end());
        }

        std::cout << "Firm " << firm << " matched agents: ";
        for (int agent : match) {
            std::cout << agent << " ";
        }
        std::cout << std::endl;
    }
    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching simple_match_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<int> agent_match(n_agents, UNMATCHED);
    while (true) {
        std::vector<int> type1_blocking_pair_list = find_best_type1_blocking_pair_list(firm_match, agent_match, agent_prefs, firm_prefs, agent_col_prefs, firm_capacities);
        bool finish = true;
        for (int firm=0; firm<n_firms; firm++) {
            if (type1_blocking_pair_list[firm]!=-1) {
                finish=false;
                break;
            }
        }
        if (finish) break;

        // 同一agentが複数firmから選ばれた場合、そのagentが最も行きたいfirmにのみ移動する
        std::map<int, int> agent_best_firm; // agent -> 移動先firm
        std::map<int, int> agent_best_score; // agent -> そのfirmでのスコア
        for (int firm=0; firm<n_firms; firm++) {
            int agent = type1_blocking_pair_list[firm];
            if (agent == -1) continue;
            int score = compute_agent_score(firm, firm_match[firm], agent_prefs[agent], agent_col_prefs[agent]);
            if (agent_best_firm.find(agent) == agent_best_firm.end() || score > agent_best_score[agent]) {
                agent_best_firm[agent] = firm;
                agent_best_score[agent] = score;
            }
        }
        for (auto [agent, firm] : agent_best_firm) {
            int old_firm = agent_match[agent];
            if (old_firm != UNMATCHED) firm_match[old_firm].erase(agent);
            firm_match[firm].insert(agent);
            agent_match[agent] = firm;
        }
    }

    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
)
{
    // Min-heap: (preference_score, agent_id) → 先頭が最もスコアの低い（最悪の）エージェント
    using MinHeap = std::priority_queue<
        std::pair<int,int>,
        std::vector<std::pair<int,int>>,
        std::greater<std::pair<int,int>>
    >;
    std::vector<MinHeap> firm_heaps(n_firms);

    std::vector<std::vector<int>> agent_order(n_agents);
    #pragma omp parallel for
    for (int a = 0; a < n_agents; ++a) {
        std::vector<int> firms(n_firms);
        std::iota(firms.begin(), firms.end(), 0);
        std::sort(firms.begin(), firms.end(), [&](int f1, int f2){
            return agent_prefs[a][f1] > agent_prefs[a][f2];
        });
        agent_order[a] = firms;
    }
    std::vector<int> next_idx(n_agents, 0);
    std::queue<int> free_agents;
    for (int a = 0; a < n_agents; ++a) free_agents.push(a);

    while (!free_agents.empty()) {
        int agent = free_agents.front();
        free_agents.pop();
        if (next_idx[agent] >= n_firms) continue;
        int firm = agent_order[agent][next_idx[agent]];
        next_idx[agent]++;

        if (firm_prefs[firm][agent] < 0) {
            free_agents.push(agent);
            continue;
        }

        if ((int)firm_heaps[firm].size() < firm_capacities[firm]) {
            // firmに空きがあれば受け入れ
            firm_heaps[firm].push({firm_prefs[firm][agent], agent});
        } else {
            // min-heapの先頭 = 最もスコアの低いエージェント → O(1)
            auto [worst_score, worst_agent] = firm_heaps[firm].top();
            if (firm_prefs[firm][agent] > worst_score) {
                firm_heaps[firm].pop();  // worst_agentを除去 → O(log C)
                firm_heaps[firm].push({firm_prefs[firm][agent], agent});  // O(log C)
                free_agents.push(worst_agent);
            } else {
                free_agents.push(agent);
            }
        }
    }

    // heapをsetに変換してマッチングを構築
    std::vector<std::set<int>> firm_match(n_firms);
    #pragma omp parallel for
    for (int f = 0; f < n_firms; ++f) {
        while (!firm_heaps[f].empty()) {
            firm_match[f].insert(firm_heaps[f].top().second);
            firm_heaps[f].pop();
        }
    }
    return Matching::from_firm_assignment(firm_match, n_agents);
}