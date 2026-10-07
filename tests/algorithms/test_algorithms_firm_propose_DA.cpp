#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
using namespace MatchingTypes;

TEST(Algorithms, FirmProposeDAFirmOptimal)
{
    const vector<vector<int>> agent_prefs = {{2, 1}, {1, 2}};
    const vector<vector<int>> firm_prefs = {{1, 2}, {2, 1}};
    const vector<vector<int>> agent_col_prefs(2, vector<int>(2, 0));
    const vector<int> capacities = {1, 1};

    MatchingSystem ms(2, 2);
    ms.set_prefs(agent_prefs, firm_prefs, agent_col_prefs);
    ms.set_firm_capacities(capacities);
    Matching m = ms.run_algorithm("firm_propose_DA");

    EXPECT_EQ(m.get_agent_matchs(), (vector<int>{1, 0}));
    EXPECT_EQ(ms.run_algorithm("doctor_propose_DA").get_agent_matchs(), (vector<int>{0, 1}));
    EXPECT_TRUE(m.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities));
    EXPECT_EQ(m.get_firm_scores(), (vector<int>{2, 2}));
}

TEST(Algorithms, FirmProposeDARefillsVacancy)
{
    const vector<vector<int>> agent_prefs = {{1, 2}, {2, 1}, {2, 1}};
    const vector<vector<int>> firm_prefs = {{3, 2, 1}, {3, 1, 2}};
    const vector<int> capacities = {2, 1};

    Matching m = firm_proposing_DA_algorithm(3, 2, capacities, agent_prefs, firm_prefs);

    EXPECT_EQ(m.get_agent_matchs(), (vector<int>{1, 0, 0}));
    EXPECT_EQ(m.get_firm_matchs(), (vector<set<int>>{{1, 2}, {0}}));
    EXPECT_TRUE(m.is_stable(agent_prefs, firm_prefs, vector<vector<int>>(3, vector<int>(3, 0)), capacities));
}

TEST(Algorithms, FirmProposeDARejectsNegativePreferences)
{
    const vector<vector<int>> agent_prefs = {{-1, 1}, {0, -1}, {3, -1}, {2, 1}};
    const vector<vector<int>> firm_prefs = {{4, 3, -1, 2}, {-1, -1, 2, -1}};

    Matching m = firm_proposing_DA_algorithm(4, 2, {2, 1}, agent_prefs, firm_prefs);

    EXPECT_EQ(m.get_agent_matchs(), (vector<int>{UNMATCHED, 0, UNMATCHED, 0}));
    EXPECT_TRUE(m.is_individually_rational(agent_prefs, firm_prefs, vector<vector<int>>(4, vector<int>(4, 0))));
}

TEST(Algorithms, FirmProposeDAZeroCapacityAndExhaustedProposals)
{
    const vector<vector<int>> agent_prefs = {{3, 2, 1}, {3, 2, 1}};
    const vector<vector<int>> firm_prefs = {{2, 1}, {2, 1}, {2, 1}};

    Matching m = firm_proposing_DA_algorithm(2, 3, {0, 3, 1}, agent_prefs, firm_prefs);

    EXPECT_EQ(m.get_firm_matchs(), (vector<set<int>>{{}, {0, 1}, {}}));
    EXPECT_EQ(m.get_agent_matchs(), (vector<int>{1, 1}));
}

TEST(Algorithms, FirmProposeDATiesKeepCurrentFirm)
{
    const vector<vector<int>> agent_prefs = {{1, 1}, {1, 1}, {1, 1}};
    const vector<vector<int>> firm_prefs = {{1, 1, 1}, {1, 1, 1}};

    Matching m = firm_proposing_DA_algorithm(3, 2, {1, 1}, agent_prefs, firm_prefs);

    EXPECT_EQ(m.get_agent_matchs(), (vector<int>{0, 1, UNMATCHED}));
}

TEST(Algorithms, FirmProposeDAAcceptsZeroScores)
{
    Matching m = firm_proposing_DA_algorithm(1, 1, {1}, {{0}}, {{0}});

    EXPECT_EQ(m.get_agent_matchs(), (vector<int>{0}));
}

TEST(Algorithms, FirmProposeDAEmptyMarket)
{
    Matching no_agents = firm_proposing_DA_algorithm(0, 2, {1, 1}, {}, {{}, {}});
    EXPECT_TRUE(no_agents.get_agent_matchs().empty());
    EXPECT_EQ(no_agents.get_firm_matchs(), (vector<set<int>>(2)));

    Matching no_firms = firm_proposing_DA_algorithm(2, 0, {}, {{}, {}}, {});
    EXPECT_EQ(no_firms.get_agent_matchs(), (vector<int>{UNMATCHED, UNMATCHED}));
    EXPECT_TRUE(no_firms.get_firm_matchs().empty());
}

TEST(Algorithms, FirmProposeDAStabilityWithNumericPreferences)
{
    const int n_agents = 5;
    const int n_firms = 3;
    const vector<vector<int>> agent_col_prefs(n_agents, vector<int>(n_agents, 0));
    mt19937 rng(42);
    uniform_int_distribution<int> score_dist(-2, 3);
    uniform_int_distribution<int> capacity_dist(0, 3);

    for (int trial = 0; trial < 200; ++trial) {
        SCOPED_TRACE(trial);
        vector<vector<int>> agent_prefs(n_agents, vector<int>(n_firms));
        vector<vector<int>> firm_prefs(n_firms, vector<int>(n_agents));
        vector<int> capacities(n_firms);
        for (auto &prefs : agent_prefs) {
            for (int &score : prefs) score = score_dist(rng);
        }
        for (auto &prefs : firm_prefs) {
            for (int &score : prefs) score = score_dist(rng);
        }
        for (int &capacity : capacities) capacity = capacity_dist(rng);

        Matching m = firm_proposing_DA_algorithm(n_agents, n_firms, capacities, agent_prefs, firm_prefs);
        EXPECT_TRUE(m.is_stable(agent_prefs, firm_prefs, agent_col_prefs, capacities));
        for (int firm = 0; firm < n_firms; ++firm) {
            EXPECT_LE((int)m.get_firm_matchs()[firm].size(), capacities[firm]);
        }
    }
}

TEST(Algorithms, FirmProposeDAOptimalAmongAllStableMatchings)
{
    for (unsigned int seed = 0; seed < 50; ++seed) {
        SCOPED_TRACE(seed);
        MatchingSystem ms(4, 3);
        ms.generate_random_prefs(PrefKind::ranked, seed, nullopt, PrefKind::none);
        const vector<int> capacities = {(int)(seed % 3), 1, 2};
        ms.set_firm_capacities(capacities);
        Matching m = ms.run_algorithm("firm_propose_DA");
        ASSERT_TRUE(m.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities));

        for (auto &candidate : ms.make_all_matchings()) {
            if (!candidate.is_stable(ms.get_agent_prefs(), ms.get_firm_prefs(), ms.get_agent_col_prefs(), capacities)) continue;
            for (int firm = 0; firm < 3; ++firm) {
                EXPECT_GE(m.get_firm_scores()[firm], candidate.get_firm_scores()[firm]);
            }
        }
    }
}
