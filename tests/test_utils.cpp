#include <gtest/gtest.h>
#include "Utils.hpp"

using namespace std;

TEST(UtilTest, GenerateCombinations_Test) {
    const vector<int> elems = {0, 1, 2, 3};
    const int k = 2;
    const int expected_size = 6;
    auto result = generate_combinations(elems, k);
    EXPECT_EQ(result.size(), expected_size);

    set<set<int>> st = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };
    for (auto cmb : result) {
        if (!st.count(cmb)) {
            cout << "Not found " << endl;
        }
    }
}

TEST(UtilTest, GenerateAllSubsetsBySize) {
    const vector<int> elems = {0, 1, 2, 3};
    const int k = 2;

    const int expected_size = 11;
    auto result = generate_all_subsets_by_size(elems, k);

    set<set<int>> st0 = {
        {},
    };

    set<set<int>> st1 = {
        {0},
        {1},
        {2},
        {3},
    };

    set<set<int>> st2 = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };

    vector<set<set<int>>> st = {st0, st1, st2};

    int sum = 0;
    for (auto [x, subset] : result) {
        set<set<int>> tmp = st[x];
        for (auto S : subset) {
            if (!tmp.count(S)) cout << "Not found" << endl;
        }
        sum += subset.size();
    }

    EXPECT_EQ(expected_size, sum);
}

TEST(UtilTest, PrepareAllCandidates) {
    const vector<int> agent_ids = {0, 1, 2, 3};
    const vector<int> firm_capacities = {3, 2};

    auto result = prepare_all_candidates(agent_ids, firm_capacities);

    set<set<int>> result1(result[0].begin(), result[0].end());
    set<set<int>> result2(result[1].begin(), result[1].end());

    set<set<int>> st0 = {
        {},
    };

    set<set<int>> st1 = {
        {0},
        {1},
        {2},
        {3},
    };

    set<set<int>> st2 = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };

    set<set<int>> st3 = {
        {0, 1, 2},
        {0, 1, 3},
        {0, 2, 3},
        {1, 2, 3},
    };

    set<set<int>> merged1 = st0;
    merged1.insert(st1.begin(), st1.end());
    merged1.insert(st2.begin(), st2.end());
    merged1.insert(st3.begin(), st3.end());

    set<set<int>> merged2 = st0;
    merged2.insert(st1.begin(), st1.end());
    merged2.insert(st2.begin(), st2.end());

    for (auto tmp : result1) {
        if (!merged1.count(tmp)) cout << "Not found" << endl;
    }
    for (auto tmp : result2) {
        if (!merged2.count(tmp)) cout << "Not found" << endl;
    }
    EXPECT_EQ(result1.size(), merged1.size());
    EXPECT_EQ(result2.size(), merged2.size());
}

TEST(UtilTest, GenerateMatchingsRecursive) {
    int n_agents = 3;
    const vector<int> firm_capacities = {1, 2};

    vector<int> agent_ids(n_agents);
    iota(agent_ids.begin(), agent_ids.end(), 0);

    auto all_candidates = prepare_all_candidates(agent_ids, firm_capacities);

    vector<Matching> result;
    vector<std::set<int>> current_matching;
    set<int> used_agents;
    generate_matchings_recursive(0, n_agents, all_candidates, current_matching, used_agents, result);

    int expected_size = 19;

    EXPECT_EQ(expected_size, result.size());

    int i = 0;
    for (auto& matching : result) {
        cout << "Matching" << i << endl;

        auto agent_match = matching.get_agent_match();
        cout << "---- agents ----" << endl;
        for (int i=0; i<n_agents; i++) {
            cout << "agent" << i << " = " << agent_match[i] << endl;
        }
        cout << agent_match.size() << endl;
        cout << endl;

        auto firm_match = matching.get_firm_match();
        cout << "---- firms ----" << endl;
        for (int i=0; i<firm_capacities.size(); i++) {
            cout << "firm" << i << " = ";
            for (auto _agent : firm_match[i]) {
                cout << _agent << ", ";
            }
            cout << endl;
        }
        cout << endl;

        auto agent_col_match = matching.get_agent_col_match();
        cout << "---- agent_cols ----" << endl;
        for (int i=0; i<n_agents; i++) {
            cout << "agent" << i << " = ";
            for (auto _agent : agent_col_match[i]) {
                cout << _agent << ", ";
            }
            cout << endl;
        }
        cout << endl;
        i++;
    }
}