#include <gtest/gtest.h>
#include "Matching.hpp"
#include "CommonTypes.hpp"

using namespace std;
using namespace MatchingTypes;

// n_firm = 1, |mu(f)| == cap(f)のときunmatchのagentのweak blocking pairを認識するか
TEST(MatchingTest, WeaklyBlockingPairs01) {
    vector<int> agent_match = {0, 0, UNMATCHED};

    vector<set<int>> firm_match = {
        {0, 1}, // firm0 -> agent 0, 1
    };

    vector<vector<int>> agent_prefs = {
        {5},
        {2},
        {4}
    };
    vector<vector<int>> firm_prefs = {
        {2, 1, 7},
    };
    vector<vector<int>> agent_col_prefs = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };

    vector<int> firm_capacities = {2};
    Matching m = Matching(agent_match, firm_match);
    auto weakly_blocking_pairs = m.weakly_blocking_pairs(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities);
    vector<pair<int,int>> expect = {
        {2,0}
    };
    EXPECT_EQ(weakly_blocking_pairs, expect);
}

// n_firm = 2でmatchedのagentのweak blocking pairを認識するか
TEST(MatchingTest, WeaklyBlockingPairs02) {
    vector<int> agent_match = {0, 0, 1};

    vector<set<int>> firm_match = {
        {0, 1}, // firm0 -> agent 0, 1
        {2}
    };

    vector<vector<int>> agent_prefs = {
        {5, 3},
        {2, 1},
        {4, 1}
    };
    vector<vector<int>> firm_prefs = {
        {2, 1, 7},
        {3, 4, 5}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };

    vector<int> firm_capacities = {2, 1};
    Matching m = Matching(agent_match, firm_match);
    auto weakly_blocking_pairs = m.weakly_blocking_pairs(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities);
    vector<pair<int,int>> expect = {
        {2,0}
    };
    EXPECT_EQ(weakly_blocking_pairs, expect);
}

// n_firm = 1, |mu(f)| < cap(f)のときunmatchのagentのweak blocking pairを認識するか
TEST(MatchingTest, WeaklyBlockingPairs03) {
    vector<int> agent_match = {UNMATCHED, 0, UNMATCHED};

    vector<set<int>> firm_match = {
        {1, 2}, // firm0 -> agent 0, 1
    };

    vector<vector<int>> agent_prefs = {
        {5},
        {2},
        {4}
    };
    vector<vector<int>> firm_prefs = {
        {2, 1, 7},
    };
    vector<vector<int>> agent_col_prefs = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };

    vector<int> firm_capacities = {2};
    Matching m = Matching(agent_match, firm_match);
    auto weakly_blocking_pairs = m.weakly_blocking_pairs(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities);
    vector<pair<int,int>> expect = {
        {0,0},
        {2,0}
    };
    EXPECT_EQ(weakly_blocking_pairs, expect);
}

