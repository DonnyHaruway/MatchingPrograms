#include "algorithmsUtils.hpp"

std::pair<bool, bool> is_acceptable(int agent, int firm,
    const std::vector<std::vector<int>>& matching,
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

