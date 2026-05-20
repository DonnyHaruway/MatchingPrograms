#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include "algorithmsUtils.hpp"

using namespace std;

TEST(AlgorithmsUtils, CreateAllFirmMatchingSubsets) {
    vector<set<int>> firm_match = {
        {0, 1, 2},
        {3, 4},
        {5}
    };

    vector<int> firm_capacities = {3, 2, 1};

    vector<FirmMatching> result = create_current_firm_match_subsets(firm_match, firm_capacities);
    vector<FirmMatching> expected = {
        {0, {}}, {0, {0}}, {0, {1}}, {0, {2}}, {0, {0, 1}}, {0, {1, 2}}, {0, {0, 2}},
        {1, {}}, {1, {3}}, {1, {4}}, 
        {2, {}},
    };

    EXPECT_THAT(result, ::testing::UnorderedElementsAreArray(expected));
}

TEST(AlgorithmsUtils, FindPreferedMatch) {
    vector<set<int>> firm_match = {
        {1, 2},
        {3, 4},
    };
    vector<int> firm_capacities = {2, 3};
    vector<FirmMatching> all_firm_match = create_current_firm_match_subsets(firm_match, firm_capacities);
    int agent = 0;
    vector<int> agent_pref = {2, 3};
    vector<int> agent_col_pref = {0, 3, 5, 1, 2};
    set<FirmMatching> unofferable = {
        {0, {2}}
    };
    FirmMatching prefered_match = find_prefered_match(agent, agent_pref, agent_col_pref, all_firm_match, unofferable);
    FirmMatching expected = {
        1, {3, 4}
    };
    FirmMatching notExpected = {
        0, {2}
    };

    EXPECT_EQ(prefered_match, expected);
    EXPECT_NE(prefered_match, notExpected);
}

TEST(AlgorithmsUtils, IsFirmMatchAcceptPropose) {
    set<int> firm_match_before = {1, 2};
    int firm_capacity = 3;
}

// TEST(AlgorithmsUtils, ExcludeOneAgent) {
//     /*
//     ・輪講で扱った例
//         firm  -> {0 : 坂東}
//         agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
//     */
//     int agent = 3;
//     int firm = 0;
//     vector<set<int>> matching = {
//         {0, 2}
//     };
//     vector<vector<int>> agent_prefs = {
//         {3}, {3}, {3}, {3}
//     };
//     vector<vector<int>> firm_prefs = {
//         {2, 4, 1, 3},
//     };
//     vector<vector<int>> agent_col_prefs = {
//         {0, -3, 3, 3},
//         {1, 0, 3, 5},
//         {7, -5, 0, 4},
//         {4, 1, 3, 0}
//     };
//     int capacity = 2;
//     int excluded = exclude_one_agent(agent, firm, matching, agent_prefs, firm_prefs, agent_col_prefs, capacity);
//     cout << excluded << endl;
//     EXPECT_TRUE(excluded == 2);
// }

// TEST(AlgorithmsUtils, exclude_one_agent_case2) {
//     /*
//     ・輪講で扱った例
//         firm  -> {0 : 坂東}
//         agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
//     */
//     int agent = 1;
//     int firm = 0;
//     vector<set<int>> matching = {
//         {0, 3}
//     };
//     vector<vector<int>> agent_prefs = {
//         {3}, {3}, {3}, {3}
//     };
//     vector<vector<int>> firm_prefs = {
//         {2, 4, 1, 3},
//     };
//     vector<vector<int>> agent_col_prefs = {
//         {0, -3, 3, 3},
//         {1, 0, 3, 5},
//         {7, -5, 0, 4},
//         {4, 1, 3, 0}
//     };
//     int capacity = 2;
//     int excluded = exclude_one_agent(agent, firm, matching, agent_prefs, firm_prefs, agent_col_prefs, capacity);
//     cout << excluded << '\n';
//     EXPECT_TRUE(excluded == -1);
// }

// TEST(AlgorithmsUtils, exclude_one_agent_case3) {
//     /*
//     ・輪講で扱った例
//         firm  -> {0 : 坂東}
//         agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
//     */
//     int agent = 1;
//     int firm = 0;
//     vector<set<int>> matching = {
//         {0, 3}
//     };
//     vector<vector<int>> agent_prefs = {
//         {3}, {3}, {3}, {3}
//     };
//     vector<vector<int>> firm_prefs = {
//         {2, 4, 1, 3},
//     };
//     vector<vector<int>> agent_col_prefs = {
//         {0, -3, 3, 3},
//         {1, 0, 3, 5},
//         {7, -5, 0, 4},
//         {4, 5, 3, 0}
//     };
//     int capacity = 2;
//     int excluded = exclude_one_agent(agent, firm, matching, agent_prefs, firm_prefs, agent_col_prefs, capacity);
//     cout << excluded << '\n';
//     EXPECT_TRUE(excluded == 0);
// }