#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
using namespace MatchingTypes;
TEST(PreferenceGenerationTest, AgentPreferenceSizeCorrect) {
    vector<int> cap = {2, 2};
    MatchingSystem ms(3, 2);
    ms.generate_random_prefs(PrefKind::ranked, 42);  // seed付き

    const auto& prefs = ms.get_agent_prefs();

    // エージェント数 = 3
    EXPECT_EQ(prefs.size(), 3);
    // 各エージェントが持つ選好リストのサイズ = 企業数 = 2
    for (const auto& pref_list : prefs) {
        EXPECT_EQ(pref_list.size(), 2);
    }
}

TEST(PreferenceGenerationTest, ValuesInRangeForRandomScores) {
    vector<int> cap = {3, 3, 2, 2};
    MatchingSystem ms(4, 3);
    ms.generate_random_prefs(PrefKind::numeric, 99, nullopt, nullopt, 10, 20, 0, 100, 0, 100);  // agent score min-max = [10,20]

    const auto& prefs = ms.get_agent_prefs();

    for (const auto& pref_list : prefs) {
        for (int firm_score : pref_list) {
            EXPECT_GE(firm_score, 10);
            EXPECT_LE(firm_score, 20);
        }
    }
}

// agent/firm/col の3種類が同じ乱数列から作られていないことを確認する
TEST(PreferenceGenerationTest, ThreePreferencesAreIndependent) {
    const int n = 4;
    MatchingSystem ms(n, n);
    ms.generate_random_prefs(PrefKind::numeric, 42, nullopt, PrefKind::numeric, 0, 100, 0, 100, 0, 100);

    EXPECT_NE(ms.get_agent_prefs(), ms.get_firm_prefs());
    EXPECT_NE(ms.get_agent_prefs(), ms.get_agent_col_prefs());
    EXPECT_NE(ms.get_firm_prefs(), ms.get_agent_col_prefs());
}

// 同じseedなら同じ選好が再現される
TEST(PreferenceGenerationTest, SameSeedIsReproducible) {
    MatchingSystem ms1(4, 3), ms2(4, 3);
    ms1.generate_random_prefs(PrefKind::ranked, 2024);
    ms2.generate_random_prefs(PrefKind::ranked, 2024);

    EXPECT_EQ(ms1.get_agent_prefs(), ms2.get_agent_prefs());
    EXPECT_EQ(ms1.get_firm_prefs(), ms2.get_firm_prefs());
    EXPECT_EQ(ms1.get_agent_col_prefs(), ms2.get_agent_col_prefs());
}

// col_kind に PrefKind::none を渡すと同僚選好はすべて0になる
TEST(PreferenceGenerationTest, NoneColKindGivesZeroPrefs) {
    const int n_agents = 4;
    MatchingSystem ms(n_agents, 2);
    ms.generate_random_prefs(PrefKind::ranked, 1, nullopt, PrefKind::none);

    for (const auto& pref_list : ms.get_agent_col_prefs()) {
        EXPECT_EQ(pref_list, vector<int>(n_agents, 0));
    }
}

// col_kind を省略すると agent_kind と同じ方式で同僚選好が作られる
TEST(PreferenceGenerationTest, ColKindDefaultsToAgentKind) {
    const int n_agents = 4;
    MatchingSystem ms(n_agents, 2);
    ms.generate_random_prefs(PrefKind::ranked, 1);

    const auto& col_prefs = ms.get_agent_col_prefs();
    for (int i = 0; i < n_agents; ++i) {
        // 自分自身は0、他人は1..n_agents-1 の順位付け
        EXPECT_EQ(col_prefs[i][i], 0);
        vector<int> sorted = col_prefs[i];
        sort(sorted.begin(), sorted.end());
        EXPECT_EQ(sorted, (vector<int>{0, 1, 2, 3}));
    }
}

// binary は同僚選好専用
TEST(PreferenceGenerationTest, BinaryIsRejectedForOpponentPrefs) {
    MatchingSystem ms(3, 2);
    EXPECT_THROW(ms.generate_random_agent_prefs(PrefKind::binary, 1), invalid_argument);
    EXPECT_THROW(ms.generate_random_firm_prefs(PrefKind::binary, 1), invalid_argument);
    EXPECT_THROW(ms.generate_random_agent_prefs(PrefKind::none, 1), invalid_argument);
}