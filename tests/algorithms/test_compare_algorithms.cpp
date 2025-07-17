#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

TEST(Algorithms, DictatorVSdoctorProposeDA)
{
    const int n_agents = 4;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};
    const vector<vector<int>> agent_col_prefs = {
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    };
    int cnt = 0;
    for (int i = 0; i < 100; i++)
    {   
        MatchingSystem ms(n_agents, n_firms, capacities);
        ms.generate_prefs("ranked", 42);
        ms.set_agent_col_prefs(agent_col_prefs);
        auto firm_prefs = ms.get_firm_prefs();
        Matching m1 = ms.run_algorithm("dictator");
        Matching m2 = ms.run_algorithm("doctor_propose_DA");
        m1.print();
        m2.print();
        if (m1==m2) cnt++;
    }
    cout << cnt << endl;
}