#include "../../include/MatchingSystem.hpp"

#include <iostream>
#include <vector>

int main()
{
    const int N_AGENTS = 4;
    const int N_FIRMS  = 2;
    const std::vector<int> CAPACITIES = {2, 2};

    // iter=395 の再現ケース
    // Agent Preferences (agent->firm):
    //   Agent 0: firm0=1, firm1=2
    //   Agent 1: firm0=2, firm1=1
    //   Agent 2: firm0=1, firm1=2
    //   Agent 3: firm0=2, firm1=1
    const std::vector<std::vector<int>> agent_prefs = {
        {1, 2},
        {2, 1},
        {1, 2},
        {2, 1},
    };

    // Firm Preferences (firm->agent):
    //   Firm 0: agent0=1, agent1=2, agent2=4, agent3=3
    //   Firm 1: agent0=2, agent1=4, agent2=1, agent3=3
    const std::vector<std::vector<int>> firm_prefs = {
        {1, 2, 4, 3},
        {2, 4, 1, 3},
    };

    // Agent Colleague Preferences (binary: 0 or -1000000000):
    //   Agent 0: [0, -1e9, 0, 0]   ← agent1が嫌い
    //   Agent 1: [0,  0,   0, 0]   ← 誰でもOK
    //   Agent 2: [0,  0,   0, -1e9] ← agent3が嫌い
    //   Agent 3: [0, -1e9, 0, 0]   ← agent1が嫌い
    const std::vector<std::vector<int>> agent_col_prefs = {
        { 0, -1000000000,           0,           0},
        { 0,           0,           0,           0},
        { 0,           0,           0, -1000000000},
        { 0, -1000000000,           0,           0},
    };

    MatchingSystem ms(N_AGENTS, N_FIRMS);
    ms.set_agent_prefs(agent_prefs);
    ms.set_firm_prefs(firm_prefs);
    ms.set_agent_col_prefs(agent_col_prefs);
    ms.set_firm_capacities(CAPACITIES);

    std::cout << "--- preferences ---\n";
    ms.print_prefs();

    std::cout << "--- running simple_match_algorithm ---\n";
    Matching m = ms.run_algorithm("simple_match");

    std::cout << "--- matching result ---\n";
    m.print();

    bool is_ws = m.is_weakly_stable(
        ms.get_agent_prefs(),
        ms.get_firm_prefs(),
        ms.get_agent_col_prefs(),
        ms.get_firm_capacities()
    );
    std::cout << "is_weakly_stable: " << (is_ws ? "true" : "false") << "\n";

    auto wbp = m.weakly_blocking_pairs(
        ms.get_agent_prefs(),
        ms.get_firm_prefs(),
        ms.get_agent_col_prefs(),
        ms.get_firm_capacities()
    );
    std::cout << "weakly blocking pairs (" << wbp.size() << "):\n";
    for (auto [agent, firm] : wbp) {
        std::cout << "  agent=" << agent << " firm=" << firm << "\n";
    }

    return 0;
}
