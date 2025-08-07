#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;

void print_2d_vector(const vector<vector<int>> &vec, const string &name)
{
    cout << name << ":\n";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        cout << "  [" << i << "]: ";
        for (size_t j = 0; j < vec[i].size(); ++j)
        {
            cout << vec[i][j];
            if (j != vec[i].size() - 1)
                cout << ", ";
        }
        cout << '\n';
    }
    cout << endl;
}

TEST(Algorithms, DictatorLikeAlgorithm1)
{
    /*
    ・輪講で扱った例
        firm  -> {0 : 坂東}
        agent -> {0 : 清水, 1: 高瀬, 2: 小久保, 3: X}
    */

    const int n_agents = 4;
    const int n_firms = 1;
    const vector<int> capacities = {2};

    vector<vector<int>> agent_prefs = {
        {3},
        {3},
        {3},
        {3}};
    vector<vector<int>> firm_prefs = {
        {2, 4, 1, 3},
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -3, 3, 6},
        {2, 0, 3, 2},
        {3, -5, 0, 2},
        {4, 5, 3, 0}
    };

    MatchingSystem ms(n_agents, n_firms, capacities);

    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    Matching matching = ms.run_algorithm("dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
}

// TEST(Algorithms, DictatorLikeAlgorithms2)
// {
//     const int n_agents = 4;
//     const int n_firms = 2;
//     const vector<int> capacities = {2, 2};
//     MatchingSystem ms(n_agents, n_firms, capacities);
//     ms.generate_prefs("numeric", 42);
//     const vector<vector<int>> agent_prefs = ms.get_agent_prefs();
//     const vector<vector<int>> firm_prefs = ms.get_firm_prefs();
//     const vector<vector<int>> agent_col_prefs = ms.get_agent_col_prefs();
//     print_2d_vector(agent_prefs, "agent_prefs");
//     print_2d_vector(firm_prefs, "firm_prefs");
//     print_2d_vector(agent_col_prefs, "agent_col_prefs");

//     for (int i = 0; i < 100; i++)
//     {
//         cout << "Run #" << i << '\n';
//         Matching matching = ms.run_algorithm("dictator");
//         matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
//         matching.print();
//     }
// }