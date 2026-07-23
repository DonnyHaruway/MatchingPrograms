#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
using namespace MatchingTypes;

TEST(Algorithms, DictatorVSdoctorProposeDA)
{
    const int n_agents = 4;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};
    int cnt = 0;
    for (int i = 0; i < 1e5; i++)
    {
        MatchingSystem ms(n_agents, n_firms);
        random_device rd;
        ms.generate_random_prefs(PrefKind::ranked, rd(), nullopt, PrefKind::none, 1, 10, 1, 10);
        ms.set_firm_capacities(capacities);
        auto firm_prefs = ms.get_firm_prefs();
        Matching m1 = ms.run_algorithm("doctor_dictator");
        Matching m2 = ms.run_algorithm("doctor_propose_DA"); 
        if (m1 == m2)
            cnt++;
    }
    cout << "same result count = " << cnt << endl;
}