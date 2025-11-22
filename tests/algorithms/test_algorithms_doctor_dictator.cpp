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
    Matching matching = ms.run_algorithm("doctor_dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
}

TEST(Algorithms, DictatorLikeAlgorithm2)
{
    const int n_agents = 3;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};

    vector<vector<int>> agent_prefs = {
        {9, 3},
        {10, 1},
        {9, 2}
    };

    vector<vector<int>> firm_prefs = {
        {4, 0, 4},
        {5, 6, 4},
    };

    vector<vector<int>> agent_col_prefs = {
        {0, -5, -9},
        {-2, 0, -4},
        {-10, 0, 0}
    };

    MatchingSystem ms(n_agents, n_firms);

    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    ms.set_firm_capacities(capacities);
    Matching matching = ms.run_algorithm("doctor_dictator");
    matching.compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    matching.print();
    if (matching.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities)) {
        cout << "The matching is stable." << endl;
    } else {
        cout << "The matching is not stable." << endl;
    }
}

TEST(Algorithms, DictatorLikeAlgorithm_RankedRandomTest1)
{
    const int n_agents = 3;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};
    random_device rd;
    int cnt = 0;
    int iter = 1e4;
    for (int i = 0; i < iter; i++)
    {
        MatchingSystem ms(n_agents, n_firms);
        ms.set_firm_capacities(capacities);
        // マッチ相手に対する選好はranked
        ms.generate_random_prefs("ranked", rd() ^ (time(NULL)+i), true, 1, 10, 1, 10, -10, 0);
        // 同僚に対する選考はnumeric, negative values
        ms.generate_random_agent_col_prefs("numeric", rd() ^ (time(NULL)+i), true, -10, 0);

        Matching m = ms.run_algorithm("doctor_dictator");

        if (m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            cnt++;
        } else {
            cout << "Trial " << i << ":\n";
            cout << "The matching is not stable." << endl;
            ms.print_prefs();
            m.print();
            auto blocking_pairs_list = m.blocking_pairs(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities);
            cout << "Blocking pairs:\n";
            for (auto [a, f] : blocking_pairs_list) {
                cout << "  Agent " << a << " and Firm " << f << "\n";
            }
            cout << endl << endl;
        }
    }

    cout << "stable matchings: " << cnt << " / " << iter << endl;
}

TEST(Algorithms, DictatorLikeAlgorithm_RankedRandomTest2)
{
    const int n_agents = 5;
    const int n_firms = 2;
    const vector<int> capacities = {4, 4};
    random_device rd;
    int cnt = 0;
    int iter = 1e4;
    for (int i = 0; i < iter; i++)
    {
        MatchingSystem ms(n_agents, n_firms);
        ms.set_firm_capacities(capacities);
        // マッチ相手に対する選好はranked
        ms.generate_random_prefs("ranked", rd() ^ (time(NULL)+i), true, 1, 10, 1, 10, -10, 0);
        // 同僚に対する選考はnumeric, negative values
        ms.generate_random_agent_col_prefs("numeric", rd() ^ (time(NULL)+i), true, -10, 0);

        Matching m = ms.run_algorithm("doctor_dictator");

        if (m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            cnt++;
        } else {
            cout << "Trial " << i << ":\n";
            cout << "The matching is not stable." << endl;
            ms.print_prefs();
            m.print();
            auto blocking_pairs_list = m.blocking_pairs(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities);
            cout << "Blocking pairs:\n";
            for (auto [a, f] : blocking_pairs_list) {
                cout << "  Agent " << a << " and Firm " << f << "\n";
            }
            cout << endl << endl;
        }
    }

    cout << "stable matchings: " << cnt << " / " << iter << endl;
}