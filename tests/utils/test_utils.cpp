#include <gtest/gtest.h>
#include "Utils.hpp"

using namespace std;

TEST(UtilsTest, GenerateCombinations_Test) {
    const vector<int> elems = {0, 1, 2, 3};
    const int k = 2;
    const int expected_size = 6;
    auto result = generate_combinations(elems, k);
    EXPECT_EQ(result.size(), expected_size);

    set<set<int>> st = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };
    for (auto cmb : result) {
        if (!st.count(cmb)) {
            cout << "Not found " << endl;
        }
    }
}

TEST(UtilsTest, GenerateAllSubsetsBySize) {
    const vector<int> elems = {0, 1, 2, 3};
    const int k = 2;

    const int expected_size = 11;
    auto result = generate_all_subsets_by_size(elems, k);

    set<set<int>> st0 = {
        {},
    };

    set<set<int>> st1 = {
        {0},
        {1},
        {2},
        {3},
    };

    set<set<int>> st2 = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };

    vector<set<set<int>>> st = {st0, st1, st2};

    int sum = 0;
    for (auto [x, subset] : result) {
        set<set<int>> tmp = st[x];
        for (auto S : subset) {
            if (!tmp.count(S)) cout << "Not found" << endl;
        }
        sum += subset.size();
    }

    EXPECT_EQ(expected_size, sum);
}

TEST(UtilsTest, GenerateAllSubsetsBySize_MaxSize) {
    const vector<int> elems = {0, 1, 2, 3};
    const int k = 5;

    const int expected_size = 16;
    auto result = generate_all_subsets_by_size(elems, k);

    set<set<int>> st0 = {
        {},
    };

    set<set<int>> st1 = {
        {0},
        {1},
        {2},
        {3},
    };

    set<set<int>> st2 = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };

    set<set<int>> st3 = {
        {0, 1, 2},
        {0, 1, 3},
        {0, 2, 3},
        {1, 2, 3},
    };

    set<set<int>> st4 = {
        {0, 1, 2, 3}
    };

    vector<set<set<int>>> st = {st0, st1, st2, st3, st4};

    int sum = 0;
    for (auto [x, subset] : result) {
        set<set<int>> tmp = st[x];
        for (auto S : subset) {
            if (!tmp.count(S)) cout << "Not found" << endl;
        }
        sum += subset.size();
    }

    EXPECT_EQ(expected_size, sum);
}

TEST(UtilsTest, GenerateAllSubsetsBySize_Empty) {
    const vector<int> elems = {};
    const int k = 5;

    const int expected_size = 1;
    auto result = generate_all_subsets_by_size(elems, k);

    set<set<int>> st0 = {
        {},
    };

    vector<set<set<int>>> st = {st0};

    int sum = 0;
    for (auto [x, subset] : result) {
        set<set<int>> tmp = st[x];
        for (auto S : subset) {
            if (!tmp.count(S)) cout << "Not found" << endl;
        }
        sum += subset.size();
    }

    EXPECT_EQ(expected_size, sum);
}

TEST(UtilsTest, PrepareAllCandidates) {
    const vector<int> agent_ids = {0, 1, 2, 3};
    const vector<int> firm_capacities = {3, 2};

    auto result = prepare_all_candidates(agent_ids, firm_capacities);

    set<set<int>> result1(result[0].begin(), result[0].end());
    set<set<int>> result2(result[1].begin(), result[1].end());

    set<set<int>> st0 = {
        {},
    };

    set<set<int>> st1 = {
        {0},
        {1},
        {2},
        {3},
    };

    set<set<int>> st2 = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };

    set<set<int>> st3 = {
        {0, 1, 2},
        {0, 1, 3},
        {0, 2, 3},
        {1, 2, 3},
    };

    set<set<int>> merged1 = st0;
    merged1.insert(st1.begin(), st1.end());
    merged1.insert(st2.begin(), st2.end());
    merged1.insert(st3.begin(), st3.end());

    set<set<int>> merged2 = st0;
    merged2.insert(st1.begin(), st1.end());
    merged2.insert(st2.begin(), st2.end());

    for (auto tmp : result1) {
        if (!merged1.count(tmp)) cout << "Not found" << endl;
    }
    for (auto tmp : result2) {
        if (!merged2.count(tmp)) cout << "Not found" << endl;
    }
    EXPECT_EQ(result1.size(), merged1.size());
    EXPECT_EQ(result2.size(), merged2.size());
}

TEST(UtilsTest, ComputeFirmScore) {
    // 簡単な例
    set<int> firm_match = {0, 2, 3};
    vector<int> firm_pref = {2, 3, 4, 5, 6};

    int score = compute_firm_score(firm_match, firm_pref);
    EXPECT_EQ(score, 11);
}

TEST(UtilsTest, ComputeAgentScore) {
    int firm = 2;
    set<int> firm_match = {1, 2, 3};
    vector<int> agent_pref = {2, 3, 4, 5, 6};
    vector<int> agent_col_pref = {-3, 0, 6, 2, 4};
    
    int score = compute_agent_score(firm, firm_match, agent_pref, agent_col_pref);
    EXPECT_EQ(score, 12);
}

TEST(UtilsTest, AgentScoresMap) {
    int firm = 2;
    set<int> firm_matching = {1, 3, 2};
    vector<vector<int>> agent_prefs = {
        {3, 5, 2},
        {1, 8, 3},
        {4, 9, 7},
        {6, 7, 1}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, 3, 4, 9},
        {2, 0, -5, 8},
        {3, 8, 0, -2},
        {1, 5, 4, 0}
    };

    map<int,int> expected_mp = {
        {1, 6},
        {2, 13},
        {3, 10}
    };

    map<int,int> agent_score_mp = agent_scores_mp(
        firm,
        firm_matching,
        agent_prefs,
        agent_col_prefs
    );

    EXPECT_EQ(expected_mp, agent_score_mp);
}