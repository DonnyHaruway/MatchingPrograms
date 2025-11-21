#include <gtest/gtest.h>
#include <iostream>
#include "algorithmsUtils.hpp"

using namespace std;

void printFirmMatchings(const vector<FirmMatching>& matchings) {
    for (const auto& [firm_id, agents] : matchings) {
        cout << "Firm " << firm_id << " matched with agents: ";
        for (int agent_id : agents) {
            cout << agent_id << " ";
        }
        cout << endl;
    }
}

TEST(AlgorithmsUtils, CreateAllFirmMatching) {
    vector<set<int>> firm_match = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    vector<int> firm_capacities = {3, 3, 4};

    vector<FirmMatching> all_firm_match = create_current_firm_match_subsets(firm_match, firm_capacities);
    printFirmMatchings(all_firm_match);
}

TEST(AlgorithmsUtils, FindPreferedMatch) {
    vector<set<int>> firm_match = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };
    vector<int> firm_capacities = {3, 3, 4};
    vector<FirmMatching> all_firm_match = create_current_firm_match_subsets(firm_match, firm_capacities);
    int agent = 0;
    vector<int> agent_pref = {0, 1, 2, 3, 4};
    vector<int> agent_col_pref = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    set<FirmMatching> unofferable = {
        {2, {7, 8, 9}}
    };
    FirmMatching prefered_match = find_prefered_match(agent, agent_pref, agent_col_pref, all_firm_match, unofferable);
    FirmMatching expected = {
        1, {4, 5, 6}
    };

    EXPECT_EQ(prefered_match, expected);
}

TEST(AlgorithmsUtils, FirmMatchAcceptPropose) {
    
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