#include "MatchingSystem.hpp"
#include "Utils.hpp"
#include <algorithm>

MatchingSystem::MatchingSystem(int n_agents, int n_firms)
    : n_agents(n_agents), n_firms(n_firms) {}

void MatchingSystem::generate_preferences( 
    std::string preference_type,
    unsigned int seed,
    int agent_score_min, int agent_score_max,
    int firm_score_min, int firm_score_max,
    int agent_col_score_min, int agent_col_score_max
) {
    rng.seed(seed);

    agent_prefs.clear();
    firm_prefs.clear();
    agent_col_prefs.clear();

    // Generate agent preferences
    for (int i = 0; i < n_agents; i++) {

        std::vector<int> pref;

        if (preference_type == "ranked") {
            pref = generate_random_ranked(n_firms, rng);
        } else if (preference_type == "numeric") {
            pref = generate_random_number(n_firms, agent_score_min, agent_score_max, rng);
        }
        agent_prefs.push_back(pref);
    }

    // Generate firm preferences
    for (int i = 0; i < n_firms; i++) {

        std::vector<int> pref;

        if (preference_type == "ranked") {
            pref = generate_random_ranked(n_agents, rng);
        } else if (preference_type == "numeric") {
            pref = generate_random_number(n_agents, firm_score_min, firm_score_max, rng);
        }

        firm_prefs.push_back(pref);
    }

    // Generate agent colleague preferences
    for (int i = 0; i < n_agents; i++) {

        std::vector<int> pref;

        if (preference_type == "ranked") {
            pref = generate_random_ranked(n_agents, rng);
        } else if (preference_type == "numeric") {
            pref = generate_random_number(n_agents, agent_col_score_min, agent_col_score_max, rng);
        }
        agent_col_prefs.push_back(pref);
    }
}

const std::vector<std::vector<int>>& MatchingSystem::get_agent_preferences() const {
    return agent_prefs;
}
const std::vector<std::vector<int>>& MatchingSystem::get_firm_preferences() const {
    return firm_prefs;
}
const std::vector<std::vector<int>>& MatchingSystem::get_agent_col_preferences() const {
    return agent_col_prefs;
}

void MatchingSystem::add_preferences(
    const std::vector<std::vector<int>> &agent_pref,
    const std::vector<std::vector<int>> &firm_pref,
    std::vector<std::vector<int>> &agent_col_pref
) {
    agent_prefs = agent_pref;
    firm_prefs = firm_pref;
    agent_col_prefs = agent_col_pref;
}