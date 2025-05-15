#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std; // テストコードなのでOK！

TEST(EvaluateAllMatchingsTest, InitializesWithFirmCapacitiesInConstructor)
{
    int n_agents = 2;
    int n_firms = 1;

    // コンストラクタでfirm_capacitiesを渡す
    vector<int> firm_caps = {2}; // firm0: 2人
    MatchingSystem ms(n_agents, n_firms, firm_caps);

    vector<vector<int>> agent_prefs = {
        {1}, // agent 0
        {2}  // agent 1
    };

    vector<vector<int>> firm_prefs = {
        {2, 3}, // firm 0
    };

    vector<vector<int>> agent_col_prefs = {
        {0, 2}, // agent 0
        {3, 0}, // agent 1
    };

    // 選好をセット
    ms.add_preferences(agent_prefs, firm_prefs, agent_col_prefs);

    // マッチングとスコアを取得
    auto result = ms.evaluate_all_matchings();

    // マッチング数の確認
    ASSERT_GT(result.size(), 0);

    for (const auto &[matching, score_pair] : result)
    {
        const auto &[firm_scores, agent_scores] = score_pair;
        EXPECT_EQ(firm_scores.size(), n_firms);
        EXPECT_EQ(agent_scores.size(), n_agents);
    }

    // 期待されるマッチングセットを準備
    set<vector<pair<int, vector<int>>>> expected_matchs = {
        {{0, {}}},
        {{0, {0}}},
        {{0, {1}}},
        {{0, {0, 1}}}};

    // 期待されるマッチング＋スコアのセット
    set<pair<vector<pair<int, vector<int>>>, pair<vector<int>, vector<int>>>> expected_combined = {
        {{{0, {}}}, {{0}, {0, 0}}},
        {{{0, {0}}}, {{2}, {1, 0}}},
        {{{0, {1}}}, {{3}, {0, 2}}},
        {{{0, {0, 1}}}, {{5}, {3, 5}}}};

    // 実際のマッチング＋スコアのセット
    set<pair<vector<pair<int, vector<int>>>, pair<vector<int>, vector<int>>>> actual_combined;

    for (const auto &[matching, score_pair] : result)
    {
        actual_combined.emplace(matching, score_pair);
    }

    // 完全一致の検証
    EXPECT_EQ(actual_combined, expected_combined);
}