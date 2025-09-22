#include "MatchingSystem.hpp"
#include <algorithm>
#include <stdexcept>
#include <cassert>

MatchingSystem::MatchingSystem(int n_agents, int n_firms)
    : n_agents(n_agents), n_firms(n_firms) {}

void MatchingSystem::generate_random_prefs(
    std::string preference_type,
    unsigned int seed,
    int agent_score_min, int agent_score_max,
    int firm_score_min, int firm_score_max,
    int agent_col_score_min, int agent_col_score_max
)
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
            // whoを自分自身にする
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

void MatchingSystem::generate_random_capacities(unsigned int seed)
{
    firm_capacities.clear();
    rng.seed(seed);
    std::uniform_int_distribution<int> capacity_dist(1, n_agents); // キャパシティは1からn_agentsまでの範囲でランダムに設定
    for (int i = 0; i < n_firms; i++)
    {
        firm_capacities.push_back(capacity_dist(rng));
    }
}

void MatchingSystem::set_agent_prefs(const std::vector<std::vector<int>> &prefs)
{
    if (prefs.size() != n_agents) {
        throw std::invalid_argument("agent_prefs size mismatch");
    }
    for (auto agent_pref : prefs) {
        if (agent_pref.size() != n_firms) {
            throw std::invalid_argument("agent_prefs size mismatch");
        }
    }

    agent_prefs = prefs;
    pref_flag = true;  // 少なくとも何か1つセットされたらON
}

void MatchingSystem::set_firm_prefs(const std::vector<std::vector<int>> &prefs)
{
    if (prefs.size() != n_firms) {
        throw std::invalid_argument("firm_capacities size mismatch");
    }

    for (auto firm_pref : prefs) {
        if (firm_pref.size() != n_agents) {
            throw std::invalid_argument("firm_capacities size mismatch");
        }
    }

    firm_prefs = prefs;
    pref_flag = true;
}

void MatchingSystem::set_agent_col_prefs(const std::vector<std::vector<int>> &prefs)
{
    if (prefs.size() != n_agents) {
        throw std::invalid_argument("agent_col_prefs size mismatch");
    }
    for (auto agent_col_pref : prefs) {
        if (agent_col_pref.size() != n_agents) {
            throw std::invalid_argument("agent_col_prefs size mismatch");
        }
    }

    agent_col_prefs = prefs;
    pref_flag = true;
}

void MatchingSystem::set_prefs(
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

void MatchingSystem::set_firm_capacities(const std::vector<int> &firm_capacities)
{
    if (firm_capacities.size() != n_firms) {
        throw std::length_error("firm_capacities has wrong length");
    }
    this->firm_capacities = firm_capacities;
}

std::vector<Matching> MatchingSystem::evaluate_all_matchings() const
{
    if (!pref_flag)
    {
        throw std::runtime_error("Prefrences have not been set yet");
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
    else if (algorithm_name == "doctor_propose_DA") {
        return run_doctor_proposing_DA_algorithm(n_agents, n_firms, firm_capacities, agent_prefs, firm_prefs);
    }
    else
    {
        throw std::invalid_argument("Unknown algorithm name: " + algorithm_name);
    }
}

// アクセッサ
const std::vector<std::vector<int>> &MatchingSystem::get_agent_prefs() const
{
    return agent_prefs;
}
const std::vector<std::vector<int>> &MatchingSystem::get_firm_prefs() const
{
    return firm_prefs;
}
const std::vector<std::vector<int>> &MatchingSystem::get_agent_col_prefs() const
{
    return agent_col_prefs;
}

const std::vector<int> &MatchingSystem::get_firm_capacities() const
{
    return firm_capacities;
}