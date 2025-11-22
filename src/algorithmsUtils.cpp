#include "algorithmsUtils.hpp"
#include "CommonTypes.hpp"

using namespace MatchingTypes;

std::vector<FirmMatching> create_current_firm_match_subsets(
    const std::vector<std::set<int>> &firm_matchs, 
    const std::vector<int> &firm_capacities
)
{
    std::vector<FirmMatching> all_firm_match_subsets;
    int n_firms = firm_matchs.size();

    for (int firm=0; firm<n_firms; firm++) {
        std::vector<int> firm_match(firm_matchs[firm].begin(), firm_matchs[firm].end());
        auto subset_map = generate_all_subsets_by_size(firm_match, firm_capacities[firm]); 
        for (auto [size, subset] : subset_map) {
            if (size + 1 > firm_capacities[firm]) continue;
            for (auto set : subset) {
                all_firm_match_subsets.emplace_back(firm, set);
            }
        }
    }

    return all_firm_match_subsets;
}

FirmMatching find_prefered_match(
    const int &agent, 
    const std::vector<int> &agent_pref, 
    const std::vector<int> &agent_col_pref, 
    const std::vector<FirmMatching> &all_firm_match,
    const std::set<FirmMatching> &unofferable
) 
{
    FirmMatching prefered_match = {-1, {}};
    int score = 0;
    for (FirmMatching firm_match : all_firm_match) {
        if (unofferable.count(firm_match)) continue;
        int tmp_score = compute_agent_score(firm_match.first, firm_match.second, agent_pref, agent_col_pref);
        if (tmp_score > score) {
            score = tmp_score;
            prefered_match = firm_match;
        }
    }
    return prefered_match;
}

bool firm_match_accept_propose(
    const int &agent, 
    const FirmMatching &prefered_match,
    const std::set<int> &firm_match_before,
    const std::vector<std::vector<int>> &agent_prefs, 
    const std::vector<std::vector<int>> &firm_prefs, 
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    int firm = prefered_match.first;
    std::set<int> firm_match_after = prefered_match.second;
    firm_match_after.insert(agent);
    int firm_score_before = compute_firm_score(firm_match_before, firm_prefs[firm]);
    int firm_score_after = compute_firm_score(firm_match_after, firm_prefs[firm]);
    if (firm_score_after < firm_score_before) return false;
    for (int _agent : firm_match_after) {
        if (_agent == agent) continue;
        int score_before = compute_agent_score(firm, firm_match_before, agent_prefs[_agent], agent_col_prefs[_agent]);
        int score_after = compute_agent_score(firm, firm_match_after, agent_prefs[_agent], agent_col_prefs[_agent]);
        if (score_after < score_before) return false;
    }
    return true;
}

std::set<int> find_agent_deleted(
    const std::set<int> &firm_match_before, 
    const std::set<int> &firm_match_after
)
{
    std::set<int> agent_deleted_set;
    for (int agent : firm_match_before) {
        if (!firm_match_after.count(agent)) agent_deleted_set.insert(agent);
    }

    return agent_deleted_set;
}

bool agents_accept_match(
    const std::set<int> &candidate_set,
    const int &firm,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    for (int agent : candidate_set) {
        int score = compute_agent_score(firm, candidate_set, agent_prefs[agent], agent_col_prefs[agent]);
        if (score < 0) return false;
    }
    return true;
}