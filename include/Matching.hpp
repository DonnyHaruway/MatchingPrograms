#pragma once

#include <vector>
#include <iostream>
#include <set>

class Matching
{
private:
    std::vector<int> agent_match;               // agent[i]のマッチ先
    std::vector<std::set<int>> firm_match;      // firm[i]にマッチしたagentの集合
    std::vector<std::set<int>> agent_col_match; // agent[i]の同僚の集合
    std::vector<int> agent_scores;              // agent[i]のマッチングに対する評価
    std::vector<int> firm_scores;               // firm[i]のマッチングに対する評価
    bool scores_computed = false;

public:
    // コンストラクタ
    Matching() = default;

    Matching(const std::vector<int> &agent_match, const std::vector<std::set<int>> &firm_match);

    // Matchingオブジェクトを企業側のマッチ結果から生成
    static Matching from_firm_assignment(
        const std::vector<std::set<int>> &firmMatch,
        const int &n_agents);

    // 与えられた選好からMatchingに対する個人、企業の評価を出力する
    void compute_scores(
        const std::vector<std::vector<int>> &agent_prefs,
        const std::vector<std::vector<int>> &firm_prefs,
        const std::vector<std::vector<int>> &agent_col_prefs);

    // 出力
    void print() const;

    // 比較演算子を追加
    bool operator==(const Matching &other) const;

    // アクセッサ
    const std::vector<int> &get_agent_match() const;
    const std::vector<std::set<int>> &get_firm_match() const;
    const std::vector<std::set<int>> get_agent_col_match() const;
    const std::vector<int> &get_agent_scores() const;
    const std::vector<int> &get_firm_scores() const;
};

// クラス外に以下を追加（ヘッダ末尾など）
bool operator!=(const Matching &lhs, const Matching &rhs);