#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;


TEST(Algorithms, DictatorLikeAlgorithm1) {
    /*
    ・輪講で扱った例
        firm  -> {0 : 坂東}
        agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
    */

    const int n_agents = 4;
    const int n_firms = 1;
    const vector<int> capacities = {2};

    vector<vector<int>> agent_prefs = {
        {3},
        {3},
        {3},
        {3}
    };
    vector<vector<int>> firm_prefs = {
        {2, 4, 1, 3},
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -3, 3, 6},
        {2, 0, 3, 2},
        {3, -5, 0, 2},
        {4, 1, 3, 0}
    };

    MatchingSystem ms(n_agents, n_firms, capacities);

    ms.add_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    Matching matching = ms.run_algorithm("dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
}