#include <gtest/gtest.h>
#include "MatchingSystem.hpp"
#include "algorithms_test_helpers.hpp"
#include <ctime>

using namespace std;

TEST(Algorithms, SimpleMatchingAlgorithm)
{
    const int n_agents = 4;
    const int n_firms = 2;
    MatchingSystem ms(n_agents, n_firms);
    const int INF = 1e9;
    vector<vector<int>> agent_prefs = {
        {1, 10},
        {10, 1},
        {10, 1},
        {10, 1}
    };
    vector<vector<int>> firm_prefs = {
        {4,3,2,1},
        {3,-INF,-INF,-INF}
    };
    vector<vector<int>> agent_col_prefs = {
        {0, -INF, -INF, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    };
    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    const vector<int> capacities = {2, 1};
    ms.set_firm_capacities(capacities);
    Matching matching = ms.run_algorithm("simple_match");
    matching.print();
    if (matching.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities)) {
        cout << "The matching is stable." << endl;
    } else {
        cout << "The matching is not stable." << endl;
        auto blocking_pairs_list = matching.blocking_pairs(agent_prefs, firm_prefs, agent_col_prefs, capacities);
        cout << "Blocking pairs:\n";
        for (auto [a, f] : blocking_pairs_list) {
            cout << "  Agent " << a << " and Firm " << f << "\n";
        }
        cout << endl << endl;
    }
}


// ranked選好 (小規模)
TEST(Algorithms, SimpleMatchingAlgorithm_RankedRandomTest1)
{
    const int n_agents = 4;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};
    random_device rd;
    int cnt_stable_result = 0;   // simple_matchがstableを返した回数
    int cnt_stable_exists = 0;   // stableマッチングが存在した問題数
    int iter = 1e4;
    for (int i = 0; i < iter; i++)
    {
        MatchingSystem ms(n_agents, n_firms);
        ms.set_firm_capacities(capacities);
        ms.generate_random_prefs("ranked", rd() ^ (time(NULL) + i), true, 1, 10, 1, 10, -10, 0);
        ms.generate_random_agent_col_prefs("numeric", rd() ^ (time(NULL) + i), true, -1000, 0);

        bool exists = stable_matching_exists(ms, capacities);
        if (exists) cnt_stable_exists++;

        Matching m = ms.run_algorithm("simple_match");
        if (m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            cnt_stable_result++;
        }
    }

    cout << "stable matching exists:  " << cnt_stable_exists  << " / " << iter << endl;
    cout << "simple_match is stable:  " << cnt_stable_result  << " / " << iter << endl;
}

// 二値 (0 or -INF) の同僚選好を用いたランダムシミュレーション
TEST(Algorithms, SimpleMatchingAlgorithm_BinaryColPrefRandomTest)
{
    const int n_agents = 4;
    const int n_firms = 2;
    const vector<int> capacities = {2, 2};
    random_device rd;
    int cnt_stable_result = 0;   // simple_matchがstableを返した回数
    int cnt_stable_exists = 0;   // stableマッチングが存在した問題数
    int iter = 1e4;
    for (int i = 0; i < iter; i++)
    {
        MatchingSystem ms(n_agents, n_firms);
        ms.set_firm_capacities(capacities);
        ms.generate_random_prefs("ranked", rd() ^ (time(NULL) + i), false, 1, 10, 1, 10);
        auto col_prefs = make_binary_agent_col_prefs(n_agents, rd() ^ (time(NULL) + i));
        ms.set_agent_col_prefs(col_prefs);

        bool exists = stable_matching_exists(ms, capacities);
        if (exists) cnt_stable_exists++;

        Matching m = ms.run_algorithm("simple_match");
        if (m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            cnt_stable_result++;
        } else if (!exists) {
            cout << "=== stable matching does not exist (iter=" << i << ") ===" << endl;
            ms.print_prefs();
            cout << endl;
        }
    }

    cout << "stable matching exists:  " << cnt_stable_exists  << " / " << iter << endl;
    cout << "simple_match is stable:  " << cnt_stable_result  << " / " << iter << endl;
}

// 二値 (0 or -INF) の同僚選好を用いたランダムシミュレーション
TEST(Algorithms, SimpleMatchingAlgorithm_BinaryColPrefRandomTest_WeaklyStable)
{
    const int n_agents = 4;
    const int n_firms = 2;
    const vector<int> capacities = {1, 3};
    random_device rd;
    int cnt_stable_result = 0;   // simple_matchがstableを返した回数
    int cnt_stable_exists = 0;   // stableマッチングが存在した問題数
    int iter = 1e4;
    for (int i = 0; i < iter; i++)
    {
        MatchingSystem ms(n_agents, n_firms);
        ms.set_firm_capacities(capacities);
        ms.generate_random_prefs("ranked", rd() ^ (time(NULL) + i), false, 1, 10, 1, 10);
        auto col_prefs = make_binary_agent_col_prefs(n_agents, rd() ^ (time(NULL) + i));
        ms.set_agent_col_prefs(col_prefs);

        bool exists = weakly_stable_matching_exists(ms, capacities);
        if (exists) cnt_stable_exists++;

        if (!exists) {
            cout << "=== weakly stable matching does not exist (iter=" << i << ") ===" << endl;
            ms.print_prefs();
            cout << endl;
        }

        Matching m = ms.run_algorithm("simple_match");
        if (m.is_weakly_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) {
            cnt_stable_result++;
        }
    }

    cout << "stable matching exists:  " << cnt_stable_exists  << " / " << iter << endl;
    cout << "simple_match is stable:  " << cnt_stable_result  << " / " << iter << endl;
}