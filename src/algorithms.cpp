#include "algorithms.hpp"
#include "algorithmsUtils.hpp"
#include <queue>
#include <algorithm>
#include <random>

std::pair<
    std::vector<std::pair<int, std::vector<int>>>,
    std::pair<std::vector<int>, std::vector<int>>>
run_dictator_like_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs)
{
    // step 1: agentをランダムに並べてqueueに格納
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(agent_ids.begin(), agent_ids.end(), rng);

    std::queue<int> agent_queue;
    for (int id : agent_ids)
        agent_queue.push(id);

    // step 2: firmのマッチングを初期化
    std::vector<std::pair<int, std::vector<int>>> matching = make_matching_base(n_firms);

    std::vector<int> firm_scores(n_firms, 0);
    std::vector<int> agent_scores(n_agents, 0);

    
}