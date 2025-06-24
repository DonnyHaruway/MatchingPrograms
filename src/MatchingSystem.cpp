#include "MatchingSystem.hpp"
#include <algorithm>
#include <stdexcept>

MatchingSystem::MatchingSystem(int n_agents, int n_firms, std::vector<int> firm_capacities)
    : n_agents(n_agents), n_firms(n_firms), firm_capacities(firm_capacities) {}

void MatchingSystem::generate_prefs(
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
            pref = generate_random_ranked(n_agents, rng, "col", i);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_agents, agent_col_score_min, agent_col_score_max, rng, "col", i);
        }
        agent_col_prefs.push_back(pref);
    }

    pref_flag =true;
}

void MatchingSystem::add_prefs(
    const std::vector<std::vector<int>> &agent_pref,
    const std::vector<std::vector<int>> &firm_pref,
    const std::vector<std::vector<int>> &agent_col_pref)
{
    agent_prefs = agent_pref;
    firm_prefs = firm_pref;
    agent_col_prefs = agent_col_pref;
    pref_flag=true;
}

std::vector<Matching> MatchingSystem::evaluate_all_matchings() const
{
    if (!pref_flag) {
        throw std::runtime_error("Prefrences have not been set yet.");
    }
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);

    auto all_candidates = prepare_all_candidates(agent_ids, firm_capacities);

    // 全マッチングの列挙
    std::vector<Matching> result;
    std::vector<std::set<int>> current_matching;
    std::set<int> used_agents;

    generate_matchings_recursive(
        0,
        n_agents,
        all_candidates,
        current_matching,
        used_agents,
        result  
    );

    // 計算する
    for (Matching& matching : result) {
        matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    }

    return result;
}

Matching MatchingSystem::run_algorithm(const std::string &algorithm_name) const
{
    if (algorithm_name == "dictator")
    {
        return run_dictator_like_algorithm(n_agents, n_firms, firm_capacities, agent_prefs, firm_prefs, agent_col_prefs);
    }
    else
    {
        throw std::invalid_argument("Unknown algorithm name: " + algorithm_name);
    }
}

// アクセッサ
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