#include "MatchingSystem.hpp"
#include "Utils.hpp"
#include <algorithm>

MatchingSystem::MatchingSystem(int n_agents, int n_firms, std::vector<int> firm_capacities)
    : n_agents(n_agents), n_firms(n_firms), firm_capacities(firm_capacities) {}

void MatchingSystem::generate_preferences(
    std::string preference_type,
    unsigned int seed,
    int agent_score_min, int agent_score_max,
    int firm_score_min, int firm_score_max,
    int agent_col_score_min, int agent_col_score_max)
{
    rng.seed(seed);

    agent_prefs.clear();
    firm_prefs.clear();
    agent_col_prefs.clear();

    // Generate agent preferences
    for (int i = 0; i < n_agents; i++)
    {

        std::vector<int> pref;

        if (preference_type == "ranked")
        {
            pref = generate_random_ranked(n_firms, rng);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_firms, agent_score_min, agent_score_max, rng);
        }
        agent_prefs.push_back(pref);
    }

    // Generate firm preferences
    for (int i = 0; i < n_firms; i++)
    {

        std::vector<int> pref;

        if (preference_type == "ranked")
        {
            pref = generate_random_ranked(n_agents, rng);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_agents, firm_score_min, firm_score_max, rng);
        }

        firm_prefs.push_back(pref);
    }

    // Generate agent colleague preferences
    for (int i = 0; i < n_agents; i++)
    {

        std::vector<int> pref;

        if (preference_type == "ranked")
        {
            pref = generate_random_ranked(n_agents, rng);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_agents, agent_col_score_min, agent_col_score_max, rng);
        }
        agent_col_prefs.push_back(pref);
    }
}

const std::vector<std::vector<int>> &MatchingSystem::get_agent_preferences() const
{
    return agent_prefs;
}
const std::vector<std::vector<int>> &MatchingSystem::get_firm_preferences() const
{
    return firm_prefs;
}
const std::vector<std::vector<int>> &MatchingSystem::get_agent_col_preferences() const
{
    return agent_col_prefs;
}

void MatchingSystem::add_preferences(
    const std::vector<std::vector<int>> &agent_pref,
    const std::vector<std::vector<int>> &firm_pref,
    std::vector<std::vector<int>> &agent_col_pref)
{
    agent_prefs = agent_pref;
    firm_prefs = firm_pref;
    agent_col_prefs = agent_col_pref;
}
/// この関数は全ての可能なマッチングに対する個人と企業の評価値を出力します。
/// @return 各マッチングに対する個人、企業の評価関数
std::vector<std::pair<std::vector<int>, std::vector<int>>>
    MatchingSystem::evaluate_all_matchings() const {
    // 1. agent集合を準備
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);

    // 2. 各人数の部分集合を前計算
    int max_capacity = *std::max_element(firm_capacities.begin(), firm_capacities.end());

    // 3. 各企業の候補割り当てパターンを生成
    auto subset_map = generate_all_subsets_by_size(agent_ids, max_capacity);

    std::vector<std::vector<std::vector<int>>> all_candidates;

    for (int cap : firm_capacities) {
        std::vector<std::vector<int>> merged;
        for (int k=0; k<=cap; ++k) {
            const auto& subsets = subset_map.at(k);
            merged.insert(merged.end(), subsets.begin(), subsets.end());
        }
        all_candidates.push_back(merged);
    }

    // 4. 全マッチングの列挙
    std::vector<std::vector<std::pair<int, std::vector<int>>>> all_matchings;
    std::vector<std::pair<int, std::vector<int>>> current_matching;
    std::set<int> used_agents;

    generate_matchings_recursive(0, all_candidates, current_matching, used_agents, all_matchings);
}