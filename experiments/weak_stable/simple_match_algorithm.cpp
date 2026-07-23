#include "../../include/MatchingSystem.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

using namespace MatchingTypes;

int main()
{
    const int N_ITER   = 10000;
    const int N_AGENTS = 4;
    const int N_FIRMS  = 2;
    const std::vector<int> CAPACITIES = {2, 2};

    int not_ws_count = 0;

    for (int iter = 0; iter < N_ITER; ++iter) {
        MatchingSystem ms(N_AGENTS, N_FIRMS);

        unsigned int seed = static_cast<unsigned int>(iter);
        // agent->firm, firm->agent は 1..10 のranked
        ms.generate_random_agent_prefs(PrefKind::ranked, seed,     1, 10);
        ms.generate_random_firm_prefs (PrefKind::ranked, seed + 1, 1, 10);
        // 同僚選好はbinary (0 or -INF)
        ms.generate_random_agent_col_prefs(PrefKind::binary, seed + 2);
        ms.set_firm_capacities(CAPACITIES);

        Matching m = ms.run_algorithm("simple_match");

        bool is_ws = m.is_weakly_stable(
            ms.get_agent_prefs(),
            ms.get_firm_prefs(),
            ms.get_agent_col_prefs(),
            ms.get_firm_capacities()
        );

        if (!is_ws) {
            ++not_ws_count;

            std::cout << "=== iter=" << iter << " : NOT weakly stable ===\n";

            std::cout << "--- preferences ---\n";
            ms.print_prefs();

            std::cout << "--- matching (simple_match result) ---\n";
            m.print();

            auto wbp = m.weakly_blocking_pairs(
                ms.get_agent_prefs(),
                ms.get_firm_prefs(),
                ms.get_agent_col_prefs(),
                ms.get_firm_capacities()
            );
            std::cout << "weakly blocking pairs (" << wbp.size() << "):\n";
            for (auto [agent, firm] : wbp) {
                std::cout << "  agent=" << agent << " firm=" << firm << "\n";
            }

            // 全探索でweakly stableなマッチングを列挙
            auto all_matchings = ms.make_all_matchings();
            std::vector<Matching> ws_matchings;
            for (auto& cand : all_matchings) {
                if (cand.is_weakly_stable(
                        ms.get_agent_prefs(),
                        ms.get_firm_prefs(),
                        ms.get_agent_col_prefs(),
                        ms.get_firm_capacities())) {
                    ws_matchings.push_back(cand);
                }
            }
            std::cout << "--- weakly stable matchings (exhaustive, "
                      << ws_matchings.size() << " found) ---\n";
            for (int k = 0; k < static_cast<int>(ws_matchings.size()); ++k) {
                std::cout << "[" << k << "] ";
                ws_matchings[k].print();
            }
            std::cout << "\n";
        }
    }

    double ratio = static_cast<double>(not_ws_count) / N_ITER;
    std::cout << "not weakly stable: " << not_ws_count << " / " << N_ITER
              << "  (ratio=" << std::fixed << std::setprecision(4) << ratio << ")\n";

    return 0;
}
