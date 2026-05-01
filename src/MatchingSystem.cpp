#include "MatchingSystem.hpp"
#include <algorithm>
#include <stdexcept>
#include <cassert>

MatchingSystem::MatchingSystem(int n_agents, int n_firms)
    : n_agents(n_agents), n_firms(n_firms) {}

void MatchingSystem::generate_random_prefs(
    std::string preference_type,
    unsigned int seed,
    bool colPref,
    int agent_score_min, int agent_score_max,
    int firm_score_min, int firm_score_max,
    int agent_col_score_min, int agent_col_score_max
)
{
    agent_prefs.clear();
    firm_prefs.clear();
    agent_col_prefs.clear();

    rng.seed(seed);
    MatchingSystem::generate_random_agent_prefs(
        preference_type,
        seed,
        agent_score_min,
        agent_score_max
    );
    rng.seed(seed+1);
    MatchingSystem::generate_random_firm_prefs(
        preference_type,
        seed,
        firm_score_min,
        firm_score_max
    );
    rng.seed(seed+2);
    MatchingSystem::generate_random_agent_col_prefs(
        preference_type,
        seed,
        colPref,
        agent_col_score_min,
        agent_col_score_max
    );
}

void MatchingSystem::generate_random_agent_prefs(
    std::string preference_type,
    unsigned int seed,
    int agent_score_min, int agent_score_max
)
{
    agent_prefs.clear();
    rng.seed(seed);

    for (int i = 0; i < n_agents; ++i)
    {
        std::vector<int> pref;
        if (preference_type == "ranked")
        {
            pref = generate_random_ranked(n_firms, agent_score_min ,rng, "opponent", -1);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_firms, agent_score_min, agent_score_max, rng, "opponent", -1);
        }
        else if (preference_type == "super_increasing")
        {
            pref = generate_random_super_increasing(n_firms, rng);
        }
        else
        {
            throw std::invalid_argument("Unknown preference_type: " + preference_type);
        }
        agent_prefs.push_back(pref);
    }
}

void MatchingSystem::generate_random_firm_prefs(
    std::string preference_type,
    unsigned int seed,
    int firm_score_min, int firm_score_max
)
{
    firm_prefs.clear();
    rng.seed(seed);

    for (int i = 0; i < n_firms; ++i)
    {
        std::vector<int> pref;
        if (preference_type == "ranked")
        {
            pref = generate_random_ranked(n_agents, firm_score_min, rng, "opponent", -1);
        }
        else if (preference_type == "numeric")
        {
            pref = generate_random_number(n_agents, firm_score_min, firm_score_max, rng, "opponent", -1);
        }
        else if (preference_type == "super_increasing")
        {
            pref = generate_random_super_increasing(n_agents, rng);
        }
        else
        {
            throw std::invalid_argument("Unknown preference_type: " + preference_type);
        }
        firm_prefs.push_back(pref);
    }
}

void MatchingSystem::generate_random_agent_col_prefs(
    std::string preference_type,
    unsigned int seed,
    bool colPref,
    int agent_col_score_min, int agent_col_score_max
)
{
    agent_col_prefs.clear();
    rng.seed(seed);

    for (int i = 0; i < n_agents; ++i)
    {
        std::vector<int> pref;
        if (colPref)
        {
            if (preference_type == "ranked")
            {
                pref = generate_random_ranked(n_agents, agent_col_score_min, rng, "col", i);
            }
            else if (preference_type == "numeric")
            {
                pref = generate_random_number(n_agents, agent_col_score_min, agent_col_score_max, rng, "col", i);
            }
            else if (preference_type == "binary")
            {
                pref = generate_random_binary(n_agents, rng, "col", i);
            }
            else
            {
                throw std::invalid_argument("Unknown preference_type: " + preference_type);
            }
        }
        else
        {
            pref = std::vector<int>(n_agents, 0);
        }
        agent_col_prefs.push_back(pref);
    }
}

void MatchingSystem::set_prefs(
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs)
{
    if (agent_prefs.size() != n_agents) {
        throw std::invalid_argument("Error: agent_prefs size mismatch (expected " + std::to_string(n_agents) + ")");
    }
    for (const auto &row : agent_prefs) {
        if (row.size() != n_firms) {
            throw std::invalid_argument("Error: agent_prefs row size mismatch (expected " + std::to_string(n_firms) + ")");
        }
    }

    if (firm_prefs.size() != n_firms) {
        throw std::invalid_argument("Error: firm_prefs size mismatch (expected " + std::to_string(n_firms) + ")");
    }
    for (const auto &row : firm_prefs) {
        if (row.size() != n_agents) {
            throw std::invalid_argument("Error: firm_prefs row size mismatch (expected " + std::to_string(n_agents) + ")");
        }
    }

    if (agent_col_prefs.size() != n_agents) {
        throw std::invalid_argument("Error: agent_col_prefs size mismatch (expected " + std::to_string(n_agents) + ")");
    }
    for (const auto &row : agent_col_prefs) {
        if (row.size() != n_agents) { 
            throw std::invalid_argument("Error: agent_col_prefs row size mismatch");
        }
    }

    this->agent_prefs = agent_prefs;
    this->firm_prefs = firm_prefs;
    this->agent_col_prefs = agent_col_prefs;
}

void MatchingSystem::generate_random_capacities(unsigned int seed)
{
    firm_capacities.clear();
    rng.seed(seed);
    std::uniform_int_distribution<int> capacity_dist(1, n_agents); // キャパシティは[1,n_agents]の一様分布から生成
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
}

void MatchingSystem::set_firm_capacities(const std::vector<int> &firm_capacities)
{
    if (firm_capacities.size() != n_firms) {
        throw std::length_error("firm_capacities has wrong length");
    }
    this->firm_capacities = firm_capacities;
}

std::vector<Matching> MatchingSystem::make_all_matchings() const
{
    if (agent_prefs.empty() || firm_prefs.empty() || agent_col_prefs.empty())
    {
        throw std::runtime_error("Prefrences have not been set yet");
    }
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);

    auto candidate_map = prepare_all_candidates(agent_ids, firm_capacities);

    std::vector<Matching> result;
    std::vector<std::set<int>> current_matching;
    std::set<int> used_agents;

    generate_matchings_recursive(
        0,
        n_agents,
        candidate_map,
        current_matching,
        used_agents,
        result
    );

    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(result.size()); ++i)
    {
        result[i].compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    }
    return result;
}

Matching MatchingSystem::run_algorithm(const std::string &algorithm_name) const
{
    Matching res;
    if (agent_prefs.empty() || firm_prefs.empty() || agent_col_prefs.empty())
    {
        throw std::runtime_error("Prefrences have not been set yet");
    }
    if (algorithm_name == "doctor_dictator")
    {
        res = doctor_dictator_algorithm(n_agents, n_firms, firm_capacities, agent_prefs, firm_prefs, agent_col_prefs);
    }
    else if (algorithm_name == "firm_dictator")
    {
        res = firm_dictator_algorithm(n_agents, n_firms, firm_capacities, agent_prefs, firm_prefs, agent_col_prefs);
    }
    else if (algorithm_name == "doctor_propose_DA") {
        res = doctor_proposing_DA_algorithm(n_agents, n_firms, firm_capacities, agent_prefs, firm_prefs);
    }
    else if (algorithm_name == "simple_match") {
        res = simple_match_algorithm(n_agents, n_firms, firm_capacities, agent_prefs, firm_prefs, agent_col_prefs);
    }
    else
    {
        throw std::invalid_argument("Unknown algorithm name: " + algorithm_name);
    }
    res.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    return res;
}

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

void MatchingSystem::print_prefs() const
{
    std::cout << "Agent Preferences:\n";
    for (int i = 0; i < n_agents; i++)
    {
        std::cout << "Agent " << i << ": ";
        for (int j = 0; j < n_firms; j++)
        {
            std::cout << agent_prefs[i][j] << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\nFirm Preferences:\n";
    for (int j = 0; j < n_firms; j++)
    {
        std::cout << "Firm " << j << ": ";
        for (int i = 0; i < n_agents; i++)
        {
            std::cout << firm_prefs[j][i] << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\nAgent Colleague Preferences:\n";
    for (int i = 0; i < n_agents; i++)
    {
        std::cout << "Agent " << i << ": ";
        for (int j = 0; j < n_agents; j++)
        {
            std::cout << agent_col_prefs[i][j] << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\nFirm Capacities:\n";
    for (int j = 0; j < n_firms; j++)
    {
        std::cout << "Firm " << j << ": " << firm_capacities[j] << "\n";
    }
}