#include <gtest/gtest.h>
#include "MatchingSystem.hpp"
#include "../algorithms/algorithms_test_helpers.hpp"
#include <vector>
#include <iostream>

using namespace std;
using namespace MatchingTypes;

TEST(Research, NotStableAndWeaklyStable)
{
    const int n_agents = 4;
    const int n_firms  = 2;
    const vector<int> capacities = {2, 2};
    const int n_trials = 10000;
    random_device rd;

    int found = 0;

    for (int i = 0; i < n_trials; i++) {
        MatchingSystem ms(n_agents, n_firms);

        ms.generate_random_prefs(PrefKind::ranked, rd() ^ (time(NULL) + i), nullopt, PrefKind::none, 1, 10, 1, 10);
        auto col_prefs = make_binary_agent_col_prefs(n_agents, rd() ^ (time(NULL) + i));
        ms.set_agent_col_prefs(col_prefs);
        ms.set_firm_capacities(capacities);

        auto all_matchings = ms.make_all_matchings();

        bool stable_flag = false;
        for (auto &match : all_matchings) {
            bool stable = match.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities);
            bool weakly_stable = match.is_weakly_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities);
            if (stable) {
                stable_flag = true;
            }
        }

        if (stable_flag) continue;

        bool weakly_stable_flag = false;
        for (auto &match : all_matchings) {
            bool weakly_stable = match.is_weakly_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities);
            if (weakly_stable) {
                weakly_stable_flag = true;
            }
        }

        if (weakly_stable_flag) {
            found++;
            cout << "=== Found a case that is not stable but weakly stable ===" << endl;
            ms.print_prefs();
            cout << endl;
        }
    }

    cout << "found: " << found << " matchings over " << n_trials << " trials" << endl;
}
