#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

TEST(EvaluateAllMatchingsTest, InitializesWithFirmCapacitiesInConstructor)
{
    // 卒論のExample1を利用

    const int n_agents = 2;
    const int n_firms = 1;
    const vector<int> capacities = {2};

    MatchingSystem ms(n_agents, n_firms, capacities);
    
    const vector<vector<int>> agent_pref = {
        {-4,},
        {7,}
    };
    const vector<vector<int>> firm_pref = {
        {8, 9},
    };
    const vector<vector<int>> agent_col_pref = {
        {0, 5},
        {-10, 0},
    };

    ms.set_prefs(agent_pref, firm_pref, agent_col_pref);
    std::cout << "before evaluate_all_matchings" << std::endl;

    vector<Matching> result = ms.evaluate_all_matchings();

    std::cout << "after evaluate_all_matchings" << std::endl;

    // firm0のマッチ, idxのmap
    map<vector<int>, int> idx_map = {
        {{-1, -1}, 0},
        {{0, -1}, 1},
        {{-1, 0}, 2},
        {{0, 0}, 3}
    };

    vector<vector<int>> expected_agents = {
        {0, 0}, 
        {-4, 0},
        {0, 7},
        {1, -3}
    };
    
    vector<vector<int>> expected_firms = {
        {0}, {8}, {9}, {17}
    };

    for (const auto& matching : result) {
    std::cout << "Checking matching..." << std::endl;

    auto key = matching.get_agent_matchs();
    std::cout << "agent_match = " << ::testing::PrintToString(key) << std::endl;

    if (idx_map.find(key) == idx_map.end()) {
        std::cout << "Key not found in idx_map" << std::endl;
        FAIL() << "Unexpected agent_match: " << ::testing::PrintToString(key);
    } else {
        int idx = idx_map[key];
        std::cout << "Index matched: " << idx << std::endl;

        auto agent_scores = matching.get_agent_scores();
        auto firm_scores = matching.get_firm_scores();
        std::cout << "Agent scores: " << ::testing::PrintToString(agent_scores) << std::endl;
        std::cout << "Firm scores: " << ::testing::PrintToString(firm_scores) << std::endl;

        EXPECT_EQ(expected_agents[idx], agent_scores);
        EXPECT_EQ(expected_firms[idx], firm_scores);
    }
    }
}