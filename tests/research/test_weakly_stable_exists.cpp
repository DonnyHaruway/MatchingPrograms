#include <gtest/gtest.h>
#include "MatchingSystem.hpp"
#include "../algorithms/algorithms_test_helpers.hpp"

using namespace std;

// 弱い安定マッチングが存在しないとシミュレーションで報告されたインスタンスを検証する
TEST(Research, WeaklyStableExists_CounterCandidate)
{
    const int n_agents = 4;
    const int n_firms  = 2;
    const vector<int> capacities = {2, 2};

    vector<vector<int>> agent_prefs = {
        {1, 2},
        {2, 1},
        {2, 1},
        {1, 2}
    };
    vector<vector<int>> firm_prefs = {
        {1, 4, 2, 3},
        {2, 4, 3, 1}
    };
    vector<vector<int>> agent_col_prefs = {
        {0,                   0, 0, 0},
        {0,                   0, 0, 0},
        {BINARY_PREF_NEG_INF, 0, 0, 0},
        {BINARY_PREF_NEG_INF, 0, 0, 0}
    };

    MatchingSystem ms(n_agents, n_firms);
    ms.set_agent_prefs(agent_prefs);
    ms.set_firm_prefs(firm_prefs);
    ms.set_agent_col_prefs(agent_col_prefs);
    ms.set_firm_capacities(capacities);

    auto all_matchings = ms.make_all_matchings();
    bool exists = false;
    int idx = 0;
    for (auto &m : all_matchings) {
        if (m.is_weakly_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities)) {
            cout << "This matching is weakly stable." << endl;
            m.print();
            exists = true;
        } 
    }

    if (!exists) {
        cout << "=== weakly stable matching does NOT exist ===" << endl;
    }

    // シミュレーション結果の検証: 存在しないと報告されたケース
    EXPECT_TRUE(exists);
}
