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

TEST(DictatorLikeAlgorithm, dictatoralg2) {
    // 輪講で扱った例

    /*
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
        {2, 4, 3, 1},
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -3, 3, 3},
        {1, 0, 9, 5},
        {7, -5, 0, 4},
        {4, 1, 3, 0}
    };

    MatchingSystem ms(n_agents, n_firms, capacities);

    ms.add_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    Matching matching = ms.run_algorithm("dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
}