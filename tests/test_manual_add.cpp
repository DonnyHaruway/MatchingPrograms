#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
TEST(ManualAddTest, AddPreferences) {
    MatchingSystem ms(3, 2);
    vector<vector<int>> agent_pref = {
        {0, 1},
        {1, 0},
        {0, 1}
    };
    vector<vector<int>> firm_pref = {
        {0, 1},
        {1, 0}
    };
    vector<vector<int>> agent_col_pref = {
        {0, 1},
        {1, 0},
        {0, 1}
    };

    ms.add_preferences(agent_pref, firm_pref, agent_col_pref);

    const auto& agent_pref_ms = ms.get_agent_preferences();
    const auto& firm_pref_ms = ms.get_firm_preferences();
    const auto& agent_col_pref_ms = ms.get_agent_col_preferences();

    EXPECT_EQ(agent_pref_ms.size(), 3);
    EXPECT_EQ(firm_pref_ms.size(), 2);
    EXPECT_EQ(agent_col_pref_ms.size(), 3);

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(agent_pref_ms[i], agent_pref[i]);
        EXPECT_EQ(agent_col_pref_ms[i], agent_col_pref[i]);
    }

    for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(firm_pref_ms[i], firm_pref[i]);
    }
}