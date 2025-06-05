#include <gtest/gtest.h>
#include "Matching.hpp"

using namespace std;

TEST(MatchingTest, FromFirmAssignmentWorkers) {
    vector<vector<int>> firm_assignments = {
        {0, 1},     // firm 0 -> agent 0, 1
        {2}         // firm 1 -> agent 2
    };

    Matching m = Matching::from_firm_assignment(firm_assignments, 3);
    const auto& a2f = m.get_agent_match();
    EXPECT_EQ(a2f[0], 0);
    EXPECT_EQ(a2f[1], 0);
    EXPECT_EQ(a2f[2], 1);
}

TEST(MatchingTest, ComputeScores) {
    // 卒論のExample2を利用

    vector<int> agent_match = {0, 1, 0};

    vector<vector<int>> firm_match = {
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

    Matching m = Matching(agent_match, firm_match);
    m.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    const auto agent_scores = m.get_agent_scores();
    const auto firm_scores = m.get_firm_scores();

    EXPECT_EQ(agent_scores[0], 8);
    EXPECT_EQ(agent_scores[1], 7);
    EXPECT_EQ(agent_scores[2], 11);

    EXPECT_EQ(firm_scores[0], 9);
    EXPECT_EQ(firm_scores[1], 4);
    m.print();
}