#include "algorithmsUtils.hpp"
#include "CommonTypes.hpp"

using namespace MatchingTypes;

std::vector<FirmMatching> create_all_firm_match(
    const std::vector<std::set<int>> &firm_matchs, 
    const std::vector<int> &firm_capacities
)
{
    std::vector<FirmMatching> all_firm_match;
    int n_firms = firm_matchs.size();

    for (int firm=0; firm<n_firms; firm++) {
        std::vector<int> firm_match(firm_matchs[firm].begin(), firm_matchs[firm].end());
        auto subset_map = generate_all_subsets_by_size(firm_match, firm_capacities[firm]); 
        for (auto [size, subset] : subset_map) {
            if (size + 1 > firm_capacities[firm]) continue;
            for (auto set : subset) {
                all_firm_match.emplace_back(firm, set);
            }
        }
    }

    return all_firm_match;
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
    // std::cout << "[firm_match_accept_propose] : START" << std::endl;
    int firm = prefered_match.first;
    std::set<int> firm_match_after = prefered_match.second;
    firm_match_after.insert(agent);
    if (compute_firm_score(firm_match_after, firm_prefs[firm]) < compute_firm_score(firm_match_before, firm_prefs[firm])) return false;
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

int exclude_one_agent(
    int agent, int firm,
    const std::vector<std::set<int>> &matching,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    const int &firm_capacitiy
)
{
    int excluded_agent = -1;
    int score_diff_max = 0;
    if (matching[firm].size() == firm_capacitiy)
    {
        for (int agent_del : matching[firm])
        {
            std::set<int> matching_deleted = matching[firm];
            matching_deleted.erase(agent_del);
            matching_deleted.insert(agent);
            if (!can_swap_agents(firm, agent, agent_prefs, agent_col_prefs, matching[firm], matching_deleted)) continue;
            int score_diff_tmp = firm_prefs[firm][agent] - firm_prefs[firm][agent_del];
            for (int agent_col : matching_deleted)
            {
                if (agent_col == agent)
                    continue;
                int score_before = compute_agent_score(firm, matching[firm], agent_prefs[agent_col], agent_col_prefs[agent_col]);
                int score_after = compute_agent_score(firm, matching_deleted, agent_prefs[agent_col], agent_col_prefs[agent_col]);
                score_diff_tmp += score_after - score_before;
            }
            if (score_diff_tmp > score_diff_max)
            {
                excluded_agent = agent_del;
                score_diff_max = score_diff_tmp;
            }
        }
    }
    else
    {
        std::__throw_invalid_argument("matching[firm] must be full capacity.");
    }
    return excluded_agent;
}

bool can_swap_agents(
    int firm, int agent, 
    const std::vector<std::vector<int>> &agent_prefs, 
    const std::vector<std::vector<int>> &agent_col_prefs, 
    const std::set<int> &matching_before, 
    const std::set<int> &matching_after
)
{   
    bool can_swap = true;
    for (int agent_col : matching_after)
    {
        if (agent_col == agent)
            continue;
        int score_before = compute_agent_score(firm, matching_before, agent_prefs[agent_col], agent_col_prefs[agent_col]);
        int score_after = compute_agent_score(firm, matching_after, agent_prefs[agent_col], agent_col_prefs[agent_col]);
        if (score_after < score_before)
        {
            can_swap = false;
            break;
        };
    }
    return can_swap;
}

bool all_rejected(
    const std::vector<bool> &is_matched, 
    const std::vector<std::vector<bool>> &confess_lists
) {
    std::vector<int> unmatched_agents;
    int n_agents = is_matched.size();
    for (int agent=0; agent<n_agents; agent++) {
        if (!is_matched[agent]) unmatched_agents.push_back(agent);
    }

    for (int agent : unmatched_agents) {
        if (std::any_of(confess_lists[agent].begin(), confess_lists[agent].end(), [] (bool b) { return !b; })) return false;
    }
    return true;
}