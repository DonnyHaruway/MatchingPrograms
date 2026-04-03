#include <gtest/gtest.h>
#include "MatchingSystem.hpp"
#include "../algorithms/algorithms_test_helpers.hpp"

using namespace std;

// 弱い安定マッチングが存在しないとシミュレーションで報告されたインスタンスを検証する
TEST(Research, StableExists_CounterCandidate)
{
    const int n_agents = 4;
    const int n_firms  = 2;
    const vector<int> capacities = {1, 3};

    vector<vector<int>> agent_prefs = {
        {2, 1},
        {2, 1},
        {2, 1},
        {2, 1}
    };
    vector<vector<int>> firm_prefs = {
        {3, 4, 2, 1},
        {3, 1, 4, 2}
    };
    vector<vector<int>> agent_col_prefs = {
        {0,                   0, 0,                   0},
        {BINARY_PREF_NEG_INF, 0, BINARY_PREF_NEG_INF, BINARY_PREF_NEG_INF},
        {0,                   0, 0,                   BINARY_PREF_NEG_INF},
        {0,                   0, 0,                   0}
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
        m.print();
        if (m.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities)) {
            cout << "This matching is stable." << endl;
            exists = true;
        } 
        cout << endl;
    }

    if (!exists) {
        cout << "=== stable matching does NOT exist ===" << endl;
    }
}
