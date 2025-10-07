#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

TEST(Algorithms, DoctorProposeDA1) 
{
    const int n_agents = 3;
    const int n_firms = 2;
    const vector<int> capacities = {2,1};

    vector<vector<int>> agent_prefs = {
        {2,1},
        {1,2},
        {1,2},};
    vector<vector<int>> firm_prefs = {
        {3, 2, 1},
        {1, 3, 2}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
    };
    MatchingSystem ms(n_agents, n_firms);
    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    ms.set_firm_capacities(capacities);
    Matching m = ms.run_algorithm("doctor_propose_DA");
    m.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    EXPECT_EQ(m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), ms.get_firm_capacities()), true);
}

TEST(Algorithms, DoctorProposeDAStabilityCheck)
{
    const int n_agents = 4;
    const int n_firms = 2;
    random_device rd;

    int cnt = 0;
    for (int i = 0; i < 100; i++)
    {
        cout << "Trial " << i << endl;
        MatchingSystem ms(n_agents, n_firms);
        ms.generate_random_prefs("ranked", rd() ^ (time(NULL)+i), -10, 10, -10, 10, 0, 0);
        ms.generate_random_capacities(rd() ^ (time(NULL)+i));
        Matching m = ms.run_algorithm("doctor_propose_DA");
        m.print();
        if (m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), ms.get_firm_capacities())) cnt++;
    }

    cout << "Stable matchings found: " << cnt << " out of " << 100 << endl;
}