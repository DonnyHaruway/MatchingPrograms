#include "MatchingSystem.hpp"
#include <algorithm>
#include <stdexcept>
#include <cassert>

using namespace MatchingTypes;

namespace
{
    /// @brief seedが省略されていたらランダムな値を返す
    unsigned int resolve_seed(std::optional<unsigned int> seed)
    {
        if (seed.has_value()) return *seed;
        std::random_device rd;
        return rd();
    }

    /// @brief 相手方(opp)への選好を1人分生成する
    std::vector<int> generate_opp_pref(
        PrefKind kind,
        int size,
        std::mt19937 &rng,
        std::optional<int> score_min,
        std::optional<int> score_max
    )
    {
        switch (kind)
        {
            case PrefKind::ranked:
                return generate_random_ranked(Side::opp, size, rng, std::nullopt, score_min);
            case PrefKind::numeric:
                return generate_random_numeric(Side::opp, size, rng, std::nullopt, score_min, score_max);
            case PrefKind::super_increasing:
                return generate_random_super_increasing(Side::opp, size, rng);
            case PrefKind::binary:
                throw std::invalid_argument("PrefKind::binary is only defined for colleague preferences");
            case PrefKind::none:
                throw std::invalid_argument("PrefKind::none is not allowed for opponent preferences");
        }
        throw std::invalid_argument("Unknown PrefKind");
    }

    /// @brief 同僚(col)への選好を1人分生成する
    std::vector<int> generate_col_pref(
        PrefKind kind,
        int size,
        int who,
        std::mt19937 &rng,
        std::optional<int> score_min,
        std::optional<int> score_max
    )
    {
        switch (kind)
        {
            case PrefKind::ranked:
                return generate_random_ranked(Side::col, size, rng, who, score_min);
            case PrefKind::numeric:
                return generate_random_numeric(Side::col, size, rng, who, score_min, score_max);
            case PrefKind::super_increasing:
                return generate_random_super_increasing(Side::col, size, rng, who);
            case PrefKind::binary:
                return generate_random_binary(Side::col, size, rng, who);
            case PrefKind::none:
                return std::vector<int>(size, 0);
        }
        throw std::invalid_argument("Unknown PrefKind");
    }
}

MatchingSystem::MatchingSystem(int n_agents, int n_firms)
    : n_agents(n_agents), n_firms(n_firms) {}

void MatchingSystem::generate_random_prefs(
    PrefKind agent_kind,
    std::optional<unsigned int> seed,
    std::optional<PrefKind> firm_kind,
    std::optional<PrefKind> col_kind,
    std::optional<int> agent_score_min, std::optional<int> agent_score_max,
    std::optional<int> firm_score_min, std::optional<int> firm_score_max,
    std::optional<int> agent_col_score_min, std::optional<int> agent_col_score_max
)
{
    const unsigned int base_seed = resolve_seed(seed);

    // 3種類の選好が同じ乱数列にならないよう、シードをずらして渡す
    generate_random_agent_prefs(
        agent_kind,
        base_seed,
        agent_score_min,
        agent_score_max
    );
    generate_random_firm_prefs(
        firm_kind.value_or(agent_kind),
        base_seed + 1,
        firm_score_min,
        firm_score_max
    );
    generate_random_agent_col_prefs(
        col_kind.value_or(agent_kind),
        base_seed + 2,
        agent_col_score_min,
        agent_col_score_max
    );
}

void MatchingSystem::generate_random_agent_prefs(
    PrefKind kind,
    std::optional<unsigned int> seed,
    std::optional<int> agent_score_min, std::optional<int> agent_score_max
)
{
    agent_prefs.clear();
    rng.seed(resolve_seed(seed));

    for (int i = 0; i < n_agents; ++i)
    {
        agent_prefs.push_back(
            generate_opp_pref(kind, n_firms, rng, agent_score_min, agent_score_max)
        );
    }
}

void MatchingSystem::generate_random_firm_prefs(
    PrefKind kind,
    std::optional<unsigned int> seed,
    std::optional<int> firm_score_min, std::optional<int> firm_score_max
)
{
    firm_prefs.clear();
    rng.seed(resolve_seed(seed));

    for (int i = 0; i < n_firms; ++i)
    {
        firm_prefs.push_back(
            generate_opp_pref(kind, n_agents, rng, firm_score_min, firm_score_max)
        );
    }
}

void MatchingSystem::generate_random_agent_col_prefs(
    PrefKind kind,
    std::optional<unsigned int> seed,
    std::optional<int> agent_col_score_min, std::optional<int> agent_col_score_max
)
{
    agent_col_prefs.clear();
    rng.seed(resolve_seed(seed));

    for (int i = 0; i < n_agents; ++i)
    {
        agent_col_prefs.push_back(
            generate_col_pref(kind, n_agents, i, rng, agent_col_score_min, agent_col_score_max)
        );
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

void MatchingSystem::generate_random_capacities(std::optional<unsigned int> seed)
{
    firm_capacities.clear();
    rng.seed(resolve_seed(seed));
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