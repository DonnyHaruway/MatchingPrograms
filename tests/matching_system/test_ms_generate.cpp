#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
TEST(PreferenceGenerationTest, AgentPreferenceSizeCorrect) {
    vector<int> cap = {2, 2};
    MatchingSystem ms(3, 2);
    ms.generate_random_prefs("ranked", 42);  // seed付き

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
    ms.generate_random_prefs("numeric", 99, 10, 20, 0, 100, 0, 100);  // agent score min-max = [10,20]

    const auto& prefs = ms.get_agent_prefs();

    for (const auto& pref_list : prefs) {
        for (int firm_score : pref_list) {
            EXPECT_GE(firm_score, 10);
            EXPECT_LE(firm_score, 20);  
        }
    }
}