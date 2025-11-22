#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

TEST(Algorithms, FirmDictatorAlgorithm1)
{
    const int n_agents = 3;
    const int n_firms = 2;
    MatchingSystem ms(n_agents, n_firms);
    vector<vector<int>> agent_prefs = {
        {5, 3},
        {6, 8},
        {2, 5}
    };
    vector<vector<int>> firm_prefs = {
        {1, 8, 3},
        {4, 3, 1}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -1, 0},
        {0, 0, -10},
        {-6, -3, 0}
    };
    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    const vector<int> capacities = {2, 2};
    ms.set_firm_capacities(capacities);
    Matching matching = ms.run_algorithm("firm_dictator");
    matching.print();
    if (matching.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities)) {
        cout << "The matching is stable." << endl;
    } else {
        cout << "The matching is not stable." << endl;
        auto blocking_pairs_list = matching.blocking_pairs(agent_prefs, firm_prefs, agent_col_prefs, capacities);
        cout << "Blocking pairs:\n";
        for (auto [a, f] : blocking_pairs_list) {
            cout << "  Agent " << a << " and Firm " << f << "\n";
        }
        cout << endl << endl;
    }
}
