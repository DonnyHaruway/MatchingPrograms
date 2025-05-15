#pragma once

#include "Utils.hpp"
#include <vector>
#include <string>
#include <random>
#include <utility>

class MatchingSystem
{
public:
    // コンストラクタ
    MatchingSystem(
        int n_agents,
        int n_firms,
        std::vector<int> capacities);

    // 選好の生成
    void generate_preferences(
        std::string preference_type,
        unsigned int seed,
        int agent_score_min = 0, int agent_score_max = 10,
        int firm_score_min = 0, int firm_score_max = 10,
        int agent_col_score_min = 0, int agent_col_score_max = 10);

    // 選好の指定
    void add_preferences(
        const std::vector<std::vector<int>> &agent_pref,
        const std::vector<std::vector<int>> &firm_pref,
        std::vector<std::vector<int>> &agent_col_pref);

    // 全マッチングの評価を返す
    std::vector<std::pair<
        std::vector<std::pair<int, std::vector<int>>>,
        std::pair<std::vector<int>, std::vector<int>>>>
    evaluate_all_matchings() const;

    // 指定したアルゴリズムでのマッチングの実行
    std::vector<int> run_algorithm(const std::string &algorithm_name) const;

    // デバッグ用：選好の表示
    const std::vector<std::vector<int>> &get_agent_preferences() const;
    const std::vector<std::vector<int>> &get_firm_preferences() const;
    const std::vector<std::vector<int>> &get_agent_col_preferences() const;

private:
    int n_agents;
    int n_firms;
    std::vector<int> firm_capacities;
    std::string preference_type;
    std::mt19937 rng;

    std::vector<std::vector<int>> agent_prefs;     // 各個人の選好のリスト
    std::vector<std::vector<int>> firm_prefs;      // 各企業の選好のリスト
    std::vector<std::vector<int>> agent_col_prefs; // 各個人の同僚に対する選好のリスト
};