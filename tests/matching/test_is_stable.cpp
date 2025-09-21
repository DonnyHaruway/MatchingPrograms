#include <gtest/gtest.h>
#include "Matching.hpp"

using namespace std;

TEST(MatchingTest, IsStable1) {
    vector<int> agent_match = {0, 1, 0};

    vector<set<int>> firm_match = {
        {0, 2}, // firm0 -> agent 0, 2
        {1}     // firm1 -> agent 1
    };

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

    vector<int> firm_capacities = {2, 2};
    Matching m = Matching(agent_match, firm_match);
    EXPECT_EQ(m.is_stable(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities), false);
}

TEST(MatchingTest, IsStable2) {
    vector<int> agent_match = {0, 1, 0};

    vector<set<int>> firm_match = {
        {0, 2}, // firm0 -> agent 0, 2
        {1}     // firm1 -> agent 1
    };

    vector<vector<int>> agent_prefs = {
        {1, -1},
        {-1, 1},
        {1, -1}
    };
    vector<vector<int>> firm_prefs = {
        {1, -1, 1},
        {-1, 1, -1}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -1, 1},
        {-1, 0, -1},
        {1, -1, 0}
    };

    vector<int> firm_capacities = {2, 2};
    Matching m = Matching(agent_match, firm_match);
    EXPECT_EQ(m.is_stable(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities), true);
}