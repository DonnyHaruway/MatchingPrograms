#pragma once

#include <vector>
#include <iostream>

class Matching {
private:
    std::vector<int> agent_to_firm;                     // agent[i] のマッチ先
    std::vector<std::vector<int>> firm_to_agents;       // firm[i] にマッチしたagentの一覧

public:
    // コンストラクタ
    Matching() = default;

    Matching(const std::vector<int>& agentMatch,
             const std::vector<std::vector<int>>& firmMatch);

    // 評価関数
    double compute_stability(const std::vector<std::vector<int>>& agent_prefs,
                             const std::vector<std::vector<int>>& firm_prefs) const;

    double compute_fairness() const;
    double compute_satisfaction() const;
    
    // Matchingオブジェクトのstaticメソッドによる生成
    static Matching from_firm_assignment(
    const std::vector<std::pair<int, std::vector<int>>>& firm_to_agents_input);

    // 出力
    void print() const;

    // アクセッサ
    const std::vector<int>& get_agent_to_firm() const;
    const std::vector<std::vector<int>>& get_firm_to_agents() const;
};