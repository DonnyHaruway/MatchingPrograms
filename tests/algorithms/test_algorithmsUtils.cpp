#include <gtest/gtest.h>
#include <iostream>
#include "algorithmsUtils.hpp"

using namespace std;

TEST(AlgorithmsUtils, firm_acceptable_case1) {
    int agent = 1;
    int firm = 0;
    vector<set<int>> matching = {
        {0, 2}
    };
    vector<vector<int>> firm_prefs = {
        {2, 4, 3, 1}
    };
    int capacity = 2;

    bool result = firm_acceptable(agent, firm, matching, firm_prefs, capacity);
    EXPECT_TRUE(result);
}

TEST(AlgorithmsUtils, agent_acceptable_case1) {
    /*
    ・輪講で扱った例
        firm  -> {0 : 坂東}
        agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
    */
    int agent = 1;
    int firm = 0;
    vector<set<int>> matching = {
        {0, 2}
    };
    vector<vector<int>> agent_prefs = {
        {3}, {3}, {3}, {3}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -3, 3, 3},
        {1, 0, 3, 5},
        {7, -5, 0, 4},
        {4, 1, 3, 0}
    };
    int capacity = 2;

    bool result = agent_acceptable(agent, firm, matching, agent_prefs, agent_col_prefs, capacity);
    EXPECT_FALSE(result);
}

TEST(AlgorithmsUtils, should_reconsider_matching_case1) {
    /*
    ・輪講で扱った例
        firm  -> {0 : 坂東}
        agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
    */
    int agent = 0;
    queue<int> queue1;
    queue1.push(1); queue1.push(2);
    vector<set<int>> matching = {
        {1, 2}
    };

    vector<vector<pair<queue<int>, vector<set<int>>>>> declined(4);
    declined[0].push_back({queue1, matching});

    queue<int> query_queue;
    query_queue.push(1); query_queue.push(2);

    bool result = should_reconsider_matching(agent, query_queue, matching, declined);
    EXPECT_TRUE(result);
}

TEST(AlgorithmsUtils, exclude_one_agent_case1) {
    /*
    ・輪講で扱った例
        firm  -> {0 : 坂東}
        agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
    */
    int agent = 3;
    int firm = 0;
    vector<set<int>> matching = {
        {0, 2}
    };
    vector<vector<int>> agent_prefs = {
        {3}, {3}, {3}, {3}
    };
    vector<vector<int>> firm_prefs = {
        {2, 4, 1, 3},
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -3, 3, 3},
        {1, 0, 3, 5},
        {7, -5, 0, 4},
        {4, 1, 3, 0}
    };
    int capacity = 2;
    int excluded = exclude_one_agent(agent, firm, matching, agent_prefs, firm_prefs, agent_col_prefs, capacity);
    cout << excluded << endl;
    EXPECT_TRUE(excluded == 2);
}

TEST(AlgorithmsUtils, exclude_one_agent_case2) {
    /*
    ・輪講で扱った例
        firm  -> {0 : 坂東}
        agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
    */
    int agent = 2;
    int firm = 0;
    vector<set<int>> matching = {
        {0, 3}
    };
    vector<vector<int>> agent_prefs = {
        {3}, {3}, {3}, {3}
    };
    vector<vector<int>> firm_prefs = {
        {2, 4, 1, 3},
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -3, 3, 3},
        {1, 0, 3, 5},
        {7, -5, 0, 4},
        {4, 1, 3, 0}
    };
    int capacity = 2;
    int excluded = exclude_one_agent(agent, firm, matching, agent_prefs, firm_prefs, agent_col_prefs, capacity);
    cout << excluded << endl;
    EXPECT_TRUE(excluded == 2);
}