#pragma once

#include <vector>
#include <random>

static const int BINARY_PREF_NEG_INF = -2000000000;

/// @brief agent_col_prefs を 0 or -INF の二値で生成する
/// @param n agentの数
/// @param seed 乱数シード
/// @return agent_col_prefs[i][j]: i==j のとき必ず0, それ以外は 0 or BINARY_PREF_NEG_INF
static std::vector<std::vector<int>> make_binary_agent_col_prefs(int n, unsigned int seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 1);

    std::vector<std::vector<int>> prefs(n, std::vector<int>(n));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) {
                prefs[i][j] = 0;
            } else {
                prefs[i][j] = dist(rng) == 0 ? 0 : BINARY_PREF_NEG_INF;
            }
        }
    }
    return prefs;
}


// stable matchingが存在するかを全列挙で判定するヘルパー
static bool stable_matching_exists(const MatchingSystem &ms, const std::vector<int> &capacities)
{
    auto all_matchings = ms.make_all_matchings();
    for (auto &match : all_matchings) {
        if (match.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            return true;
        }
    }
    return false;
}

static bool weakly_stable_matching_exists(const MatchingSystem &ms, const std::vector<int> &capacities)
{
    auto all_matchings = ms.make_all_matchings();
    for (auto &match : all_matchings) {
        if (match.is_weakly_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            return true;
        }
    }
    return false;
}