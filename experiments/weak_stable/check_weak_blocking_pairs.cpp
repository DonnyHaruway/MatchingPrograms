#include "../../include/MatchingSystem.hpp"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

// check_counter_example.cpp と同じ選好の下で、
// 指定したマッチングの弱いblocking pairが何かを検証する。
//   mu1 = { f1:{w2},     f2:{w1}, f3:{w5} }  (w3, w4 は未マッチ)
//   mu2 = { f1:{w3,w4},  f2:{w1}, f3:{w5} }  (w2 は未マッチ)
//
// 出力は2段構え:
//   1. Matching::weakly_blocking_pairs() の結果 (ライブラリの定義)
//   2. 全部分集合を走査した詳細ログ (どの提携 S が誰をどう改善するか)

using namespace MatchingTypes;

namespace {

const int NEG_INF = -1000000000;

std::string agent_name(int a) { return "w" + std::to_string(a + 1); }
std::string firm_name(int f) { return "f" + std::to_string(f + 1); }

std::string set_str(const std::set<int> &s)
{
    if (s.empty()) return "{}";
    std::string out = "{";
    bool first = true;
    for (int a : s) {
        if (!first) out += ", ";
        out += agent_name(a);
        first = false;
    }
    return out + "}";
}

std::string score_str(int score)
{
    if (score <= NEG_INF / 2) return "-INF";
    return std::to_string(score);
}

// 集合 base から、agent を含み、サイズが cap 以下の部分集合をすべて列挙する。
std::vector<std::set<int>> enumerate_coalitions(const std::set<int> &base, int agent, int cap)
{
    std::vector<int> others;
    for (int a : base)
        if (a != agent) others.push_back(a);

    std::vector<std::set<int>> result;
    const int n = static_cast<int>(others.size());
    for (int mask = 0; mask < (1 << n); ++mask) {
        std::set<int> s = {agent};
        for (int i = 0; i < n; ++i)
            if (mask & (1 << i)) s.insert(others[i]);
        if (static_cast<int>(s.size()) > cap) continue;
        result.push_back(s);
    }
    return result;
}

// 1つのマッチングについて弱いblocking pairを検証して出力する。
void analyze(
    const std::string &label,
    const std::vector<std::set<int>> &firm_assignment,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    const std::vector<int> &firm_capacities
)
{
    const int n_agent = static_cast<int>(agent_prefs.size());
    const int n_firm  = static_cast<int>(firm_prefs.size());

    Matching m = Matching::from_firm_assignment(firm_assignment, n_agent);

    std::cout << "\n########## target matching: " << label << " ##########\n";
    m.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    m.print();

    const auto &agent_matchs = m.get_agent_matchs();
    const auto &firm_matchs  = m.get_firm_matchs();
    const auto &agent_scores = m.get_agent_scores();
    const auto &firm_scores  = m.get_firm_scores();

    std::cout << "individually rational: "
              << (m.is_individually_rational(agent_prefs, firm_prefs, agent_col_prefs) ? "YES" : "NO")
              << "\n";

    // ---- 1. ライブラリ定義による弱いblocking pair ----
    auto wbp = m.weakly_blocking_pairs(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities);
    std::cout << "\n=== weakly_blocking_pairs() [library] ===\n";
    if (wbp.empty()) {
        std::cout << "  (none) -> this matching is weakly stable\n";
    } else {
        for (const auto &[a, f] : wbp)
            std::cout << "  (" << agent_name(a) << ", " << firm_name(f) << ")\n";
    }

    // ---- 2. 全提携を走査した詳細検証 ----
    // (a, f) が弱いblocking pairである条件:
    //   ある S ⊆ mu(f) ∪ {a}, a ∈ S, |S| <= q_f が存在して
    //     - f は S を mu(f) より真に選好する
    //     - a は (f, S\{a}) を現状より真に選好する
    //     - S \ {a} の各同僚は現状以上 (弱い改善で可)
    std::cout << "\n=== detailed scan over all coalitions ===\n";
    std::vector<std::pair<int, int>> detailed_wbp;

    for (int firm = 0; firm < n_firm; ++firm) {
        std::set<int> base(firm_matchs[firm].begin(), firm_matchs[firm].end());

        for (int agent = 0; agent < n_agent; ++agent) {
            if (agent_matchs[agent] == firm) continue;

            std::set<int> base_with_agent = base;
            base_with_agent.insert(agent);

            bool blocked_here = false;
            for (const auto &S : enumerate_coalitions(base_with_agent, agent, firm_capacities[firm])) {
                const int firm_after  = compute_firm_score(S, firm_prefs[firm]);
                const int agent_after = compute_agent_score(firm, S, agent_prefs[agent], agent_col_prefs[agent]);

                std::string reason;
                if (firm_after <= firm_scores[firm]) {
                    reason = "firm not better (" + score_str(firm_scores[firm]) + " -> "
                             + score_str(firm_after) + ")";
                } else if (agent_after <= agent_scores[agent]) {
                    reason = std::string(agent_name(agent)) + " not better ("
                             + score_str(agent_scores[agent]) + " -> " + score_str(agent_after) + ")";
                } else {
                    for (int col : S) {
                        if (col == agent) continue;
                        const int col_after =
                            compute_agent_score(firm, S, agent_prefs[col], agent_col_prefs[col]);
                        if (col_after < agent_scores[col]) {
                            reason = std::string(agent_name(col)) + " worse off ("
                                     + score_str(agent_scores[col]) + " -> " + score_str(col_after) + ")";
                            break;
                        }
                    }
                }

                std::cout << "  " << firm_name(firm) << " with S=" << set_str(S) << " : ";
                if (reason.empty()) {
                    std::cout << "BLOCKS  (firm " << score_str(firm_scores[firm]) << " -> "
                              << score_str(firm_after) << ", " << agent_name(agent) << " "
                              << score_str(agent_scores[agent]) << " -> " << score_str(agent_after)
                              << ")\n";
                    blocked_here = true;
                } else {
                    std::cout << "no  [" << reason << "]\n";
                }
            }

            if (blocked_here) detailed_wbp.emplace_back(agent, firm);
        }
    }

    std::cout << "\n=== weakly blocking pairs [detailed scan] ===\n";
    if (detailed_wbp.empty()) {
        std::cout << "  (none)\n";
    } else {
        for (const auto &[a, f] : detailed_wbp)
            std::cout << "  (" << agent_name(a) << ", " << firm_name(f) << ")\n";
    }

    std::cout << "\nis_weakly_stable: "
              << (m.is_weakly_stable(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities) ? "YES" : "NO")
              << "\n";
}

} // namespace

int main()
{
    const int N_AGENTS = 5; // w1..w5 -> agent 0..4
    const int N_FIRMS  = 3; // f1..f3 -> firm 0..2
    const std::vector<int> CAPACITIES = {2, 1, 1}; // q1=2, q2=1, q3=1

    // check_counter_example.cpp と同一の選好
    //   w1: f2 > f3 > f1 / w2: f3 > f1 > f2 / w3: f3 > f2 > f1
    //   w4: f3 > f2 > f1 / w5: f1 > f3 > f2
    const std::vector<std::vector<int>> agent_prefs = {
        {1, 4, 2}, // w1
        {2, 1, 4}, // w2
        {1, 2, 4}, // w3
        {1, 2, 4}, // w4
        {4, 1, 2}, // w5
    };

    //   f1: w1 > w2 > w3 > w4 > w5
    //   f2: w2 > w5 > w1 > w4 > w3
    //   f3: w5 > w1 > w2 > w3 > w4
    const std::vector<std::vector<int>> firm_prefs = {
        {16, 8, 4, 2, 1}, // f1
        {4, 16, 1, 2, 8}, // f2
        {8, 4, 2, 1, 16}, // f3
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

    // 対象マッチング1: f1 <- {w2}, f2 <- {w1}, f3 <- {w5}  (w3, w4 は未マッチ)
    analyze(
        "{f1,w2}, {f2,w1}, {f3,w5}",
        {
            {1}, // f1 <- w2
            {0}, // f2 <- w1
            {4}, // f3 <- w5
        },
        agent_prefs, firm_prefs, agent_col_prefs, CAPACITIES
    );

    // 対象マッチング2: f1 <- {w3, w4}, f2 <- {w1}, f3 <- {w5}  (w2 は未マッチ)
    analyze(
        "{f1,w3,w4}, {f2,w1}, {f3,w5}",
        {
            {2, 3}, // f1 <- w3, w4
            {0},    // f2 <- w1
            {4},    // f3 <- w5
        },
        agent_prefs, firm_prefs, agent_col_prefs, CAPACITIES
    );

    return 0;
}
