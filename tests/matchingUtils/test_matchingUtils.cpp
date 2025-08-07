#include <gtest/gtest.h>
#include "Utils.hpp"
#include "MatchingUtils.hpp"

using namespace std;

TEST(MatchingUtilsTest, GenerateMatchingsRecursive) {
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

        auto agent_match = matching.get_agent_matchs();
        cout << "---- agents ----" << endl;
        for (int i=0; i<n_agents; i++) {
            cout << "agent" << i << " = " << agent_match[i] << endl;
        }
        cout << agent_match.size() << endl;
        cout << endl;

        auto firm_match = matching.get_firm_matchs();
        cout << "---- firms ----" << endl;
        for (int i=0; i<firm_capacities.size(); i++) {
            cout << "firm" << i << " = ";
            for (auto _agent : firm_match[i]) {
                cout << _agent << ", ";
            }
            cout << endl;
        }
        cout << endl;

        auto agent_col_match = matching.get_agent_col_matchs();
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