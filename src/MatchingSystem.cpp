#include "MatchingSystem.hpp"
#include <algorithm>
#include <stdexcept>
#include <cassert>

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
            pref = generate_random_ranked(n_firms, rng, "opponent", -1);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_firms, agent_score_min, agent_score_max, rng, "opponent", -1);
        }
        agent_prefs.push_back(pref);
    }

    // Generate firm preferences
    for (int i = 0; i < n_firms; i++)
    {

        std::vector<int> pref;

        if (preference_type == "ranked")
        {
            pref = generate_random_ranked(n_agents, rng, "opponent", -1);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_agents, firm_score_min, firm_score_max, rng, "opponent", -1);
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

    pref_flag = true;
}

void MatchingSystem::add_prefs(
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs)
{
    assert(agent_prefs.size() == n_agents);
    for (const auto &row : agent_prefs)
        assert(row.size() == n_firms);

    assert(firm_prefs.size() == n_firms);
    for (const auto &row : firm_prefs)
        assert(row.size() == n_agents);

    assert(agent_col_prefs.size() == n_agents);
    for (const auto &row : agent_col_prefs)
        assert(row.size() == n_agents);

    this->agent_prefs = agent_prefs;
    this->firm_prefs = firm_prefs;
    this->agent_col_prefs = agent_col_prefs;
}
std::vector<Matching> MatchingSystem::evaluate_all_matchings() const
{
    if (!pref_flag)
    {
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
        result);

    // 計算する
    for (Matching &matching : result)
    {
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