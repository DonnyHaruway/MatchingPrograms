#include "../../include/MatchingSystem.hpp"

#include <iostream>
#include <vector>

// counterexample.pdf (Theorem 1) の例で弱安定マッチングが存在するかを全列挙で調べる。
// 順位選好はスコアに変換する: 最下位=1, 順位が1つ上がるごとに2倍 (1,2,4,8,...)。
// B = {{w2,w3},{w2,w4}} はバイナリ同僚選好 (両立不可ペアを相互に-∞) として表現する。

int main()
{
    const int N_AGENTS = 5; // w1..w5 -> agent 0..4
    const int N_FIRMS  = 3; // f1..f3 -> firm 0..2
    const std::vector<int> CAPACITIES = {2, 1, 1}; // q1=2, q2=1, q3=1

    const int NEG_INF = -1000000000;

    // Workers' preferences (agent_prefs[i][j] = agent i の firm j へのスコア)
    //   w1: f2 > f3 > f1
    //   w2: f3 > f1 > f2
    //   w3: f3 > f2 > f1
    //   w4: f3 > f2 > f1
    //   w5: f1 > f3 > f2
    const std::vector<std::vector<int>> agent_prefs = {
        {1, 4, 2}, // w1
        {2, 1, 4}, // w2
        {1, 2, 4}, // w3
        {1, 2, 4}, // w4
        {4, 1, 2}, // w5
    };

    // Firms' preferences (firm_prefs[j][i] = firm j の agent i へのスコア)
    //   f1: w1 > w2 > w3 > w4 > w5
    //   f2: w2 > w5 > w1 > w4 > w3
    //   f3: w5 > w1 > w2 > w3 > w4
    const std::vector<std::vector<int>> firm_prefs = {
        {16, 8, 4, 2, 1},  // f1
        {4, 16, 1, 2, 8},  // f2
        {8, 4, 2, 1, 16},  // f3
    };

    // B = {{w2,w3},{w2,w4}} : 両立不可ペアを相互に -INF
    std::vector<std::vector<int>> agent_col_prefs(N_AGENTS, std::vector<int>(N_AGENTS, 0));
    agent_col_prefs[1][2] = NEG_INF; // w2 <-> w3
    agent_col_prefs[2][1] = NEG_INF;
    agent_col_prefs[1][3] = NEG_INF; // w2 <-> w4
    agent_col_prefs[3][1] = NEG_INF;

    MatchingSystem ms(N_AGENTS, N_FIRMS);
    ms.set_agent_prefs(agent_prefs);
    ms.set_firm_prefs(firm_prefs);
    ms.set_agent_col_prefs(agent_col_prefs);
    ms.set_firm_capacities(CAPACITIES);

    std::cout << "--- preferences ---\n";
    ms.print_prefs();

    auto all_matchings = ms.make_all_matchings();
    std::cout << "total enumerated matchings: " << all_matchings.size() << "\n";

    long long ir_count = 0;
    long long weakly_stable_count = 0;

    for (auto &m : all_matchings) {
        if (m.is_individually_rational(
                ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs())) {
            ir_count++;
        }
        if (m.is_weakly_stable(
                ms.get_agent_prefs(), ms.get_firm_prefs(),
                ms.get_agent_col_prefs(), ms.get_firm_capacities())) {
            weakly_stable_count++;
            std::cout << "--- weakly stable matching found ---\n";
            m.print();
        }
    }

    std::cout << "individually rational matchings: " << ir_count << "\n";
    std::cout << "weakly stable matchings: " << weakly_stable_count << "\n";
    std::cout << "weakly stable matching exists: "
              << (weakly_stable_count > 0 ? "YES" : "NO") << "\n";

    return 0;
}
