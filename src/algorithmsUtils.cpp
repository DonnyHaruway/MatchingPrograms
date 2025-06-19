#include "algorithmsUtils.hpp"

std::pair<bool, bool> is_acceptable(int agent, int firm,
    const std::vector<std::set<int>>& matching,
    const std::vector<std::vector<int>>& firm_prefs,
    const std::vector<std::vector<int>>& agent_col_prefs)
{
    bool acceptable_firm = firm_prefs[firm][agent] >= 0;
    bool acceptable_agent = true;

    for (int agent_col : matching[firm]) {
        if (agent_col_prefs[agent_col][agent] <= 0) {
            acceptable_agent = false;
            break;
        }
    }

    return {acceptable_firm, acceptable_agent};
}

bool should_reconsider_matching(
int agent,
const std::vector<std::set<int>>& matching,
std::map<int, std::vector<std::pair<std::queue<int>,std::vector<std::set<int>>>>>& mp_declined)
{
    auto declined_matchings = mp_declined[agent];
    for (auto [declined_queue, declined_matching] : declined_matchings) {
        if (declined_matching == matching) return true;
    }
    return false;
}