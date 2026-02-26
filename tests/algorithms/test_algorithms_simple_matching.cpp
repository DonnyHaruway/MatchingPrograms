#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

TEST(Algorithms, SimpleMatchingAlgorithm)
{
    const int n_agents = 4;
    const int n_firms = 2;
    MatchingSystem ms(n_agents, n_firms);
    const int INF = 1e9;
    vector<vector<int>> agent_prefs = {
        {1, 10},
        {10, 1},
        {10, 1},
        {10, 1}
    };
    vector<vector<int>> firm_prefs = {
        {4,3,2,1},
        {3,-INF,-INF,-INF}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -INF, -INF, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    };
    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    const vector<int> capacities = {2, 1};
    ms.set_firm_capacities(capacities);
    Matching matching = ms.run_algorithm("simple_match");
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