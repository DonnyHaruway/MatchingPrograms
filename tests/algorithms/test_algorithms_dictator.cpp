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

    MatchingSystem ms(n_agents, n_firms);

    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    ms.set_firm_capacities(capacities);
    Matching matching = ms.run_algorithm("dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
}

TEST(Algorithms, DictatorLikeAlgorithm2)
{
    /*
    ・ご指定の選好
    Agent Preferences:
    Agent 0: 9 3 
    Agent 1: 10 1 
    Agent 2: 9 2 

    Firm Preferences:
    Firm 0: 4 0 4 
    Firm 1: 5 6 4 

    Agent Colleague Preferences:
    Agent 0: 0 -5 -9 
    Agent 1: -2 0 -4 
    Agent 2: -10 0 0 

    Firm Capacities:
    Firm 0: 2
    Firm 1: 2
    */

    const int n_agents = 3;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};

    // 各エージェントの企業に対する選好
    vector<vector<int>> agent_prefs = {
        {9, 3},
        {10, 1},
        {9, 2}};

    // 各企業のワーカーに対する選好
    vector<vector<int>> firm_prefs = {
        {4, 0, 4},
        {5, 6, 4},
    };

    // 各エージェントの同僚に対する選好
    vector<vector<int>> agent_col_prefs = {
        {0, -5, -9},
        {-2, 0, -4},
        {-10, 0, 0}};

    MatchingSystem ms(n_agents, n_firms);

    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    ms.set_firm_capacities(capacities);
    Matching matching = ms.run_algorithm("dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
    if (matching.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities)) {
        cout << "The matching is stable." << endl;
    } else {
        cout << "The matching is not stable." << endl;
    }
}

TEST(Algorithms, DictatorLikeAlgorithm3)
{
    const int INF = 2e9;
    const int n_agents = 3;
    const int n_firms = 2;
    const vector<int> capacities = {2, 1};

    vector<vector<int>> agent_prefs = {
        {2, 1},
        {2, 1},
        {1, 2}
    };
    vector<vector<int>> firm_prefs = {
        {2, 1, 3},
        {1, 3, 2}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, 0, 0},
        {-INF, 0, 0},
        {-INF, 0, 0}
    };

    MatchingSystem ms(n_agents, n_firms);

    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    ms.set_firm_capacities(capacities);
    Matching matching = ms.run_algorithm("dictator");
    matching.print();
    if (matching.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities)) {
        cout << "The matching is stable." << endl;
    } else {
        cout << "The matching is not stable." << endl;
    }
}

TEST(Algorithms, DictatorLikeAlgorithm4)
{
    const int n_agents = 3;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};
    random_device rd;
    int cnt = 0;
    int iter = 1e2;
    for (int i = 0; i < iter; i++)
    {
        MatchingSystem ms(n_agents, n_firms);
        ms.set_firm_capacities(capacities);
        ms.generate_random_prefs("numeric", rd() ^ (time(NULL)+i), true, 0, 10, 0, 10, -10, 0);
        Matching m = ms.run_algorithm("dictator");
        if (m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            cnt++;
        } else {
            cout << "Unstable matching found in trial " << i << "!" << endl;
            ms.print_prefs();
            m.print();
            cout << endl << endl;
        }
    }

    cout << "[DictatorLikeAlgorithm2] stable matchings: " << cnt << " / " << iter << endl;
}