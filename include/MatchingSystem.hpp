#pragma once

#include "Utils.hpp"
#include "algorithms.hpp"
#include "Matching.hpp"
#include <vector>
#include <string>
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
    void generate_prefs(
        std::string preference_type,
        unsigned int seed,
        int agent_score_min = 0, int agent_score_max = 10,
        int firm_score_min = 0, int firm_score_max = 10,
        int agent_col_score_min = 0, int agent_col_score_max = 10);

    // 選好の指定
    void add_prefs(
        const std::vector<std::vector<int>> &agent_pref,
        const std::vector<std::vector<int>> &firm_pref,
        const std::vector<std::vector<int>> &agent_col_pref);

    /// この関数は全ての可能なマッチングに対する個人と企業の評価値を出力する。
    /// @return Matchingオブジェクトのvector(スコア計算済み)
    std::vector<Matching> evaluate_all_matchings() const;

    /// @brief アルゴリズムに従って導出されたMatchingオブジェクトを生成する。
    /// @param algorithm_name アルゴリズムの名前を入れる
    /// @return Matching
    Matching run_algorithm(const std::string &algorithm_name) const;

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
    bool pref_flag=false;

    std::vector<std::vector<int>> agent_prefs;     // agent_prefs[i][j]: agent i が firm j に対して持つスコア
    std::vector<std::vector<int>> firm_prefs;      // firm_prefs[j][i] : agent j が firm i に対して持つスコア
    std::vector<std::vector<int>> agent_col_prefs; // agent_col_prefs[i][j]: agent i が agent j に対して持つスコア
};