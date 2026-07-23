#pragma once

#include "Utils.hpp"
#include "MatchingUtils.hpp"
#include "algorithms.hpp"
#include "Matching.hpp"
#include <optional>
#include <vector>
#include <string>
#include <utility>

class MatchingSystem
{
private:
    int n_agents;
    int n_firms;
    std::mt19937 rng;

    std::vector<std::vector<int>> agent_prefs;     // agent_prefs[i][j]: agent i が firm j に対して持つスコア
    std::vector<std::vector<int>> firm_prefs;      // firm_prefs[j][i] : agent j が firm i に対して持つスコア
    std::vector<std::vector<int>> agent_col_prefs; // agent_col_prefs[i][j]: agent i が agent j に対して持つスコア

    std::vector<int> firm_capacities; // firmのキャパシティ配列

public:
    // コンストラクタ
    MatchingSystem(
        int n_agents,
        int n_firms
    );

    /*
     * generate_random_* 系の共通仕様
     *
     * 必須引数は選好の生成方式 (PrefKind) のみで、残りはすべて省略可能。
     *   seed 省略時       : std::random_device から取得する (再現性が必要なら明示すること)
     *   min/max 省略時    : DEFAULT_SCORE_MIN / DEFAULT_SCORE_MAX
     *
     * min/maxが使われるのは PrefKind::numeric のときのみ。
     *   ranked           : min のみ使用 (max は無視)
     *   binary           : min/max とも無視し、拒否スコアは -INF 固定
     *   super_increasing : min/max とも無視
     */

    /// @brief ランダムに選好を作成する
    /// @param agent_kind agent->firmの選好の生成方式
    /// @param seed 乱数シード (省略時はランダム)
    /// @param firm_kind firm->agentの選好の生成方式 (省略時は agent_kind と同じ)
    /// @param col_kind agent->agentの選好の生成方式 (省略時は agent_kind と同じ。PrefKind::noneで同僚選好なし)
    /// @param agent_score_min agent->firmのスコアの最小値
    /// @param agent_score_max agent->firmのスコアの最大値
    /// @param firm_score_min firm->agentのスコアの最小値
    /// @param firm_score_max firm->agentのスコアの最大値
    /// @param agent_col_score_min agent->agentのスコアの最小値
    /// @param agent_col_score_max agent->agentのスコアの最大値
    /// @note 3種類の選好は seed, seed+1, seed+2 から独立に生成される
    void generate_random_prefs(
        MatchingTypes::PrefKind agent_kind,
        std::optional<unsigned int> seed = std::nullopt,
        std::optional<MatchingTypes::PrefKind> firm_kind = std::nullopt,
        std::optional<MatchingTypes::PrefKind> col_kind = std::nullopt,
        std::optional<int> agent_score_min = std::nullopt, std::optional<int> agent_score_max = std::nullopt,
        std::optional<int> firm_score_min = std::nullopt, std::optional<int> firm_score_max = std::nullopt,
        std::optional<int> agent_col_score_min = std::nullopt, std::optional<int> agent_col_score_max = std::nullopt
    );

    void generate_random_agent_prefs(
        MatchingTypes::PrefKind kind,
        std::optional<unsigned int> seed = std::nullopt,
        std::optional<int> agent_score_min = std::nullopt,
        std::optional<int> agent_score_max = std::nullopt
    );

    void generate_random_firm_prefs(
        MatchingTypes::PrefKind kind,
        std::optional<unsigned int> seed = std::nullopt,
        std::optional<int> firm_score_min = std::nullopt,
        std::optional<int> firm_score_max = std::nullopt
    );

    /// @brief 同僚への選好を作成する。PrefKind::none を渡すとすべて0になる。
    void generate_random_agent_col_prefs(
        MatchingTypes::PrefKind kind,
        std::optional<unsigned int> seed = std::nullopt,
        std::optional<int> agent_col_score_min = std::nullopt,
        std::optional<int> agent_col_score_max = std::nullopt
    );

    void generate_random_capacities(std::optional<unsigned int> seed = std::nullopt);

    // 選好の指定
    void set_agent_prefs(const std::vector<std::vector<int>> &agent_prefs);
    void set_firm_prefs(const std::vector<std::vector<int>> &firm_prefs);

    /// @brief 自分自身のスコアは0である必要がある
    /// @param agent_col_prefs 
    void set_agent_col_prefs(const std::vector<std::vector<int>> &agent_col_prefs);

    void set_prefs(
        const std::vector<std::vector<int>> &agent_prefs,
        const std::vector<std::vector<int>> &firm_prefs,
        const std::vector<std::vector<int>> &agent_col_prefs
    );

    void set_firm_capacities(const std::vector<int> &firm_capacities);

    /// この関数は全ての可能なマッチングに対する個人と企業の評価値を出力する。
    /// @return Matchingオブジェクトのvector(スコア計算済み)
    std::vector<Matching> make_all_matchings() const;

    /// @brief アルゴリズムに従って導出されたMatchingオブジェクトを生成する。
    /// @param algorithm_name アルゴリズムの名前を入れる
    /// @return Matching
    Matching run_algorithm(const std::string &algorithm_name) const;

    const std::vector<std::vector<int>> &get_agent_prefs() const;
    const std::vector<std::vector<int>> &get_firm_prefs() const;
    const std::vector<std::vector<int>> &get_agent_col_prefs() const;

    const std::vector<int> &get_firm_capacities() const;

    void print_prefs() const;
};