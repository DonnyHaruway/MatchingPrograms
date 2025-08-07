#include "algorithmsUtils.hpp"
#include "CommonTypes.hpp"

using namespace MatchingTypes;

std::vector<FirmMatching> create_all_firm_matching(
    const std::vector<std::set<int>> &firm_matching, 
    const std::vector<int> &firm_capacities
)
{
    std::vector<FirmMatching> all_firm_matching;
    int n_firms = firm_matching.size();

    for (int firm=0; firm<n_firms; firm++) {
        if (firm_matching[firm].size() < firm_capacities[firm]) all_firm_matching.emplace_back(firm, firm_matching[firm]);
        for (int agent_del : firm_matching[firm]) {
            std::set<int> firm_matching_del = firm_matching[firm];
            firm_matching_del.erase(agent_del);
            all_firm_matching.emplace_back(firm, firm_matching_del);
        }
    }

    return all_firm_matching;
}

FirmMatching find_prefered_match(
    const int &agent, 
    const std::vector<int> &agent_pref, 
    const std::vector<int> &agent_col_pref, 
    const std::vector<FirmMatching> &all_firm_matching,
    const std::set<FirmMatching> &unofferable
) 
{
    FirmMatching prefered_match = {-1, {}};
    int score = 0;
    for (FirmMatching firm_matching : all_firm_matching) {
        if (unofferable.count(firm_matching)) continue;
        int tmp_score = compute_agent_score(firm_matching.first, firm_matching.second, agent_pref, agent_col_pref);
        if (tmp_score > score) {
            score = tmp_score;
            prefered_match = firm_matching;
        }
    }
    return prefered_match;
}

bool firm_match_accept_propose(
    const int &agent, 
    const FirmMatching &prefered_match, 
    const std::set<int> &firm_matching_before, 
    const std::vector<std::vector<int>> &agent_prefs, 
    const std::vector<std::vector<int>> &firm_prefs, 
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    int firm = prefered_match.first;
    std::set<int> firm_matching_after = prefered_match.second;
    firm_matching_after.insert(agent);
    if (compute_firm_score(firm_matching_after, firm_prefs[firm]) < compute_firm_score(firm_matching_before, firm_prefs[firm])) return false;
    for (int _agent : firm_matching_after) {
        if (_agent == agent) continue;
        int score_before = compute_agent_score(firm, firm_matching_before, agent_prefs[_agent], agent_col_prefs[_agent]);
        int score_after = compute_agent_score(firm, firm_matching_after, agent_prefs[_agent], agent_col_prefs[_agent]);
        if (score_after < score_before) return false;
    }

    return true;
}

int find_agent_deleted(
    const std::set<int> &firm_matching_before, 
    const std::set<int> &firm_matching_after
)
{
    int agent_deleted = -1;
    for (int agent : firm_matching_before) {
        if (!firm_matching_after.count(agent)) agent_deleted = agent;
    }

    return agent_deleted;
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

bool can_swap_agents(int firm, int agent, const std::vector<std::vector<int>> &agent_prefs, const std::vector<std::vector<int>> &agent_col_prefs, const std::set<int> &matching_before, const std::set<int> &matching_after)
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

bool all_rejected(const std::vector<bool> &is_matched, const std::vector<std::vector<bool>> &confess_lists) {
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