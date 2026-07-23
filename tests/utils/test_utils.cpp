#include <gtest/gtest.h>
#include "Utils.hpp"

using namespace std;
using namespace MatchingTypes;

TEST(GenerateRandomTest, RankedOppIsPermutationFromMin) {
    mt19937 rng(42);
    const int size = 5;

    auto v = generate_random_ranked(Side::opp, size, rng); // min省略 -> DEFAULT_SCORE_MIN
    vector<int> sorted = v;
    sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, (vector<int>{1, 2, 3, 4, 5}));

    auto v2 = generate_random_ranked(Side::opp, size, rng, nullopt, 0);
    vector<int> sorted2 = v2;
    sort(sorted2.begin(), sorted2.end());
    EXPECT_EQ(sorted2, (vector<int>{0, 1, 2, 3, 4}));
}

TEST(GenerateRandomTest, RankedColHasZeroForSelf) {
    mt19937 rng(42);
    const int size = 4;
    const int who = 2;

    auto v = generate_random_ranked(Side::col, size, rng, who);
    EXPECT_EQ(v[who], 0);

    vector<int> sorted = v;
    sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, (vector<int>{0, 1, 2, 3}));
}

TEST(GenerateRandomTest, RankedColSkipsZeroForNegativeMin) {
    mt19937 rng(42);
    const int size = 4;
    const int who = 1;

    // 自分自身の0と衝突しないよう、-2,-1,1 が他人に割り当てられる (0はスキップ)
    auto v = generate_random_ranked(Side::col, size, rng, who, -2);
    EXPECT_EQ(v[who], 0);

    vector<int> sorted = v;
    sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, (vector<int>{-2, -1, 0, 1}));
}

TEST(GenerateRandomTest, NumericRespectsRange) {
    mt19937 rng(7);
    const int size = 6;
    const int who = 0;

    auto v = generate_random_numeric(Side::col, size, rng, who, -10, -5);
    EXPECT_EQ(v[who], 0);
    for (int i = 0; i < size; ++i) {
        if (i == who) continue;
        EXPECT_GE(v[i], -10);
        EXPECT_LE(v[i], -5);
    }
}

TEST(GenerateRandomTest, SuperIncreasingColLeavesSelfZero) {
    mt19937 rng(7);
    const int size = 5;
    const int who = 3;

    auto v = generate_random_super_increasing(Side::col, size, rng, who);
    EXPECT_EQ(v[who], 0);

    vector<int> sorted = v;
    sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, (vector<int>{0, 1, 2, 4, 8}));
}

TEST(GenerateRandomTest, BinaryUsesZeroOrPenalty) {
    mt19937 rng(7);
    const int size = 5;
    const int who = 4;

    auto v = generate_random_binary(Side::col, size, rng, who);
    EXPECT_EQ(v[who], 0);
    for (int x : v) {
        EXPECT_TRUE(x == 0 || x == -INF);
    }

    auto v2 = generate_random_binary(Side::col, size, rng, who, -1);
    for (int x : v2) {
        EXPECT_TRUE(x == 0 || x == -1);
    }
}

TEST(GenerateRandomTest, InvalidSideAndWhoCombinationsThrow) {
    mt19937 rng(1);
    const int size = 4;

    // opp なのに who を渡した
    EXPECT_THROW(generate_random_ranked(Side::opp, size, rng, 0), invalid_argument);
    // col なのに who がない
    EXPECT_THROW(generate_random_ranked(Side::col, size, rng), invalid_argument);
    // who が範囲外
    EXPECT_THROW(generate_random_ranked(Side::col, size, rng, size), out_of_range);
    // binary は col 専用
    EXPECT_THROW(generate_random_binary(Side::opp, size, rng), invalid_argument);
    // min_val/max_val は片方だけ指定できない
    EXPECT_THROW(generate_random_numeric(Side::opp, size, rng, nullopt, 0), invalid_argument);
    EXPECT_THROW(generate_random_numeric(Side::opp, size, rng, nullopt, 5, 1), invalid_argument);
}

TEST(GenerateRandomTest, SameSeedGivesSameResult) {
    mt19937 rng1(123), rng2(123);
    EXPECT_EQ(
        generate_random_numeric(Side::opp, 8, rng1),
        generate_random_numeric(Side::opp, 8, rng2)
    );
}

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