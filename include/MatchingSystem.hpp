#pragma once

#include "Utils.hpp"
#include "MatchingUtils.hpp"
#include "algorithms.hpp"
#include "Matching.hpp"
#include <vector>
#include <string>
#include <utility>

class MatchingSystem
{
private:
    int n_agents;
    int n_firms;
    std::vector<int> firm_capacities;
    std::string preference_type;
    std::mt19937 rng;
    bool pref_flag = false;

    std::vector<std::vector<int>> agent_prefs;     // agent_prefs[i][j]: agent i が firm j に対して持つスコア
    std::vector<std::vector<int>> firm_prefs;      // firm_prefs[j][i] : agent j が firm i に対して持つスコア
    std::vector<std::vector<int>> agent_col_prefs; // agent_col_prefs[i][j]: agent i が agent j に対して持つスコア

public:
    // コンストラクタ
    MatchingSystem(
        int n_agents,
        int n_firms,
        std::vector<int> capacities);

    /// @brief ランダムに先行を作成
    /// @param preference_type 選好のタイプを指定する -> "ranked" : 同順位なし, "numeric" : 同順位あり
    /// @param seed
    /// @param agent_score_min
    /// @param agent_score_max
    /// @param firm_score_min
    /// @param firm_score_max
    /// @param agent_col_score_min
    /// @param agent_col_score_max
    void generate_prefs(
        std::string preference_type,
        unsigned int seed,
        int agent_score_min = 0, int agent_score_max = 10,
        int firm_score_min = 0, int firm_score_max = 10,
        int agent_col_score_min = 0, int agent_col_score_max = 10);

    // 選好の指定

    void set_agent_prefs(const std::vector<std::vector<int>> &agent_prefs);
    void set_firm_prefs(const std::vector<std::vector<int>> &firm_prefs);

    /// @brief 自分自身のスコアは0である必要がある
    /// @param agent_col_prefs 
    void set_agent_col_prefs(const std::vector<std::vector<int>> &agent_col_prefs);

    void set_prefs(
        const std::vector<std::vector<int>> &agent_prefs,
        const std::vector<std::vector<int>> &firm_prefs,
        const std::vector<std::vector<int>> &agent_col_prefs);

    /// この関数は全ての可能なマッチングに対する個人と企業の評価値を出力する。
    /// @return Matchingオブジェクトのvector(スコア計算済み)
    std::vector<Matching> evaluate_all_matchings() const;

    /// @brief アルゴリズムに従って導出されたMatchingオブジェクトを生成する。
    /// @param algorithm_name アルゴリズムの名前を入れる
    /// @return Matching
    Matching run_algorithm(const std::string &algorithm_name) const;

    // 選好のゲッタ
    const std::vector<std::vector<int>> &get_agent_prefs() const;
    const std::vector<std::vector<int>> &get_firm_prefs() const;
    const std::vector<std::vector<int>> &get_agent_col_prefs() const;
};