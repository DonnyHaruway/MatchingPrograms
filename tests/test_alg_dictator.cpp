#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

TEST(DictatorLikeAlgorithm, dictatorlikealg) {
    // 卒論のExample2を利用

    const int n_agents = 3;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};

    vector<vector<int>> agent_prefs = {
        {5, 2},
        {2, 7},
        {4, 4}
    };
    vector<vector<int>> firm_prefs = {
        {2, 1, 7},
        {3, 4, 8}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, 10, 3},
        {1, 0, 9},
        {7, 6, 0}
    };

    MatchingSystem ms(n_agents, n_firms, capacities);

    ms.add_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    Matching matching = ms.run_algorithm("dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
}