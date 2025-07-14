#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

TEST(Algorithms, DoctorProposeDAvsDictator) 
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

    MatchingSystem ms(n_agents, n_firms, capacities);

    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    Matching matching = ms.run_algorithm("doctor_propose_DA");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
}

TEST(Algorithms, DoctorProposeDAAlgorithm) 
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

    MatchingSystem ms(n_agents, n_firms, capacities);

    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    Matching matching_da = ms.run_algorithm("doctor_propose_DA");
    Matching matching_dict = ms.run_algorithm("dictator");
    matching_da.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching_dict.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching_da.print();
    matching_dict.print();
}