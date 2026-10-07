#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
using namespace MatchingTypes;

TEST(ModifiedWorkerDA, WorkerOptimalWithoutColleagueConflicts)
{
    const vector<vector<int>> agent_prefs = {{2, 1}, {1, 2}};
    const vector<vector<int>> firm_prefs = {{1, 2}, {2, 1}};
    const vector<vector<int>> col_prefs(2, vector<int>(2, 0));
    auto matching = modified_worker_proposing_DA_algorithm(
        2, 2, {1, 1}, agent_prefs, firm_prefs, col_prefs, 42);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{0, 1}));
    EXPECT_EQ(modified_firm_proposing_DA_algorithm(
        2, 2, {1, 1}, agent_prefs, firm_prefs, col_prefs, 42).get_agent_matchs(),
        (vector<int>{1, 0}));
}

TEST(ModifiedWorkerDA, AllOffersAreResolvedBeforeCheckingColleagueConflicts)
{
    // All three propose in round 1. DA rejects agent 1 before B starts,
    // so the rejection between agents 0 and 1 must never trigger deletion.
    vector<vector<int>> col_prefs(3, vector<int>(3, 0));
    col_prefs[0][1] = col_prefs[1][0] = -INF;
    for (unsigned int seed = 0; seed < 32; ++seed) {
        auto matching = modified_worker_proposing_DA_algorithm(
            3, 1, {2}, {{1}, {1}, {1}}, {{2, 1, 3}}, col_prefs, seed);
        EXPECT_EQ(matching.get_firm_matchs(), (vector<set<int>>{{0, 2}}));
    }
}

TEST(ModifiedWorkerDA, RejectedAndDisplacedWorkersContinueInLaterRounds)
{
    auto matching = modified_worker_proposing_DA_algorithm(
        3, 2, {1, 1}, {{2, 1}, {1, 2}, {1, 2}}, {{2, 3, 1}, {3, 1, 2}},
        vector<vector<int>>(3, vector<int>(3, 0)), 42);
    // Agent 1 is rejected by firm 1, then displaces agent 0 at firm 0.
    // Agent 0 subsequently displaces agent 2 at firm 1.
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{1, 0, UNMATCHED}));
}

TEST(ModifiedWorkerDA, DeletionIsPermanentForOneFirmAndWorkerCanJoinAnother)
{
    vector<vector<int>> col_prefs = {{0, -INF}, {-INF, 0}};
    for (unsigned int seed = 0; seed < 32; ++seed) {
        SCOPED_TRACE(seed);
        auto matching = modified_worker_proposing_DA_algorithm(
            2, 2, {2, 2}, {{2, 1}, {2, 1}}, {{2, 1}, {2, 1}}, col_prefs, seed);
        EXPECT_EQ(matching.get_firm_matchs()[0].size(), 1u);
        EXPECT_EQ(matching.get_firm_matchs()[1].size(), 1u);
        const auto &agents = matching.get_agent_matchs();
        EXPECT_NE(agents[0], agents[1]);
        EXPECT_EQ(agents[0] + agents[1], 1);
    }
}

TEST(ModifiedWorkerDA, ProcessesAllPairsAndSkipsPairsResolvedByEarlierDeletions)
{
    vector<vector<int>> col_prefs(3, vector<int>(3, 0));
    col_prefs[0][2] = col_prefs[2][0] = -INF;
    col_prefs[1][2] = col_prefs[2][1] = -INF;
    bool saw_two_retained = false, saw_both_removed = false;
    for (unsigned int seed = 0; seed < 64; ++seed) {
        SCOPED_TRACE(seed);
        auto matching = modified_worker_proposing_DA_algorithm(
            3, 1, {3}, {{1}, {1}, {1}}, {{3, 2, 1}}, col_prefs, seed);
        const auto &workers = matching.get_firm_matchs()[0];
        if (workers.count(2)) EXPECT_EQ(workers.size(), 1u);
        saw_two_retained |= workers == set<int>{0, 1};
        saw_both_removed |= workers == set<int>{2};
    }
    EXPECT_TRUE(saw_two_retained);
    EXPECT_TRUE(saw_both_removed);
}

TEST(ModifiedWorkerDA, EitherDirectionOfColleagueRejectionIsInfeasible)
{
    for (const auto &col_prefs : vector<vector<vector<int>>>{
             {{0, -INF}, {0, 0}}, {{0, 0}, {-INF, 0}}}) {
        auto matching = modified_worker_proposing_DA_algorithm(
            2, 1, {2}, {{1}, {1}}, {{2, 1}}, col_prefs, 42);
        EXPECT_EQ(matching.get_firm_matchs()[0].size(), 1u);
    }
}

TEST(ModifiedWorkerDA, TiesKeepIncumbentsAndBreakNewTiesById)
{
    auto matching = modified_worker_proposing_DA_algorithm(
        3, 2, {1, 1}, {{2, 1}, {1, 2}, {1, 2}}, {{1, 1, 1}, {1, 1, 2}},
        vector<vector<int>>(3, vector<int>(3, 0)), 42);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{0, UNMATCHED, 1}));
    auto tied = modified_worker_proposing_DA_algorithm(
        2, 2, {1, 1}, {{1, 1}, {1, 1}}, {{1, 1}, {1, 1}}, {{0, 0}, {0, 0}}, 42);
    EXPECT_EQ(tied.get_agent_matchs(), (vector<int>{0, 1}));
}

TEST(ModifiedWorkerDA, NegativePreferencesZeroCapacityAndZeroScores)
{
    auto matching = modified_worker_proposing_DA_algorithm(
        3, 2, {0, 2}, {{2, 1}, {-1, 0}, {1, 1}}, {{3, 2, 1}, {2, 0, -1}},
        vector<vector<int>>(3, vector<int>(3, 0)), 42);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{1, 1, UNMATCHED}));
    auto unacceptable = modified_worker_proposing_DA_algorithm(
        1, 1, {1}, {{-1}}, {{1}}, {{0}}, 42);
    EXPECT_EQ(unacceptable.get_agent_matchs(), (vector<int>{UNMATCHED}));
    auto zero = modified_worker_proposing_DA_algorithm(1, 1, {1}, {{0}}, {{0}}, {{0}}, 42);
    EXPECT_EQ(zero.get_agent_matchs(), (vector<int>{0}));
}

TEST(ModifiedWorkerDA, EmptyMarketsTerminate)
{
    auto no_agents = modified_worker_proposing_DA_algorithm(0, 2, {1, 1}, {}, {{}, {}}, {}, 42);
    EXPECT_TRUE(no_agents.get_agent_matchs().empty());
    EXPECT_EQ(no_agents.get_firm_matchs(), (vector<set<int>>(2)));
    auto no_firms = modified_worker_proposing_DA_algorithm(2, 0, {}, {{}, {}}, {}, {{0, 0}, {0, 0}}, 42);
    EXPECT_EQ(no_firms.get_agent_matchs(), (vector<int>{UNMATCHED, UNMATCHED}));
}

TEST(ModifiedWorkerDA, SeededDispatchIsReproducibleAndPreservesInputPreferences)
{
    MatchingSystem system(4, 2);
    const vector<vector<int>> agent_prefs = {{2, 1}, {2, 1}, {1, 2}, {1, 2}};
    const vector<vector<int>> firm_prefs = {{4, 3, 2, 1}, {4, 1, 2, 3}};
    vector<vector<int>> col_prefs(4, vector<int>(4, 0));
    col_prefs[0][1] = col_prefs[1][0] = -INF;
    col_prefs[1][2] = col_prefs[2][1] = -INF;
    system.set_prefs(agent_prefs, firm_prefs, col_prefs);
    system.set_firm_capacities({2, 1});
    const auto first = system.run_algorithm("modified_worker_propose_DA", 42);
    EXPECT_EQ(first, system.run_algorithm("modified_worker_propose_DA", 42));
    EXPECT_EQ(first, modified_worker_proposing_DA_algorithm(
        4, 2, {2, 1}, agent_prefs, firm_prefs, col_prefs, 42));
    EXPECT_EQ(system.get_agent_prefs(), agent_prefs);
    EXPECT_EQ(system.get_firm_prefs(), firm_prefs);
    EXPECT_EQ(system.get_agent_col_prefs(), col_prefs);
    EXPECT_NO_THROW(first.get_agent_scores());
    EXPECT_NO_THROW(first.get_firm_scores());
}

TEST(ModifiedWorkerDA, WithoutConflictsAgreesWithStandardWorkerDAForStrictPreferences)
{
    for (unsigned int seed = 0; seed < 100; ++seed) {
        SCOPED_TRACE(seed);
        MatchingSystem system(5, 3);
        system.generate_random_prefs(PrefKind::ranked, seed, nullopt, PrefKind::none);
        system.set_firm_capacities({1 + static_cast<int>(seed % 3), 1, 2});
        auto matching = system.run_algorithm("modified_worker_propose_DA", seed);
        EXPECT_EQ(matching, system.run_algorithm("doctor_propose_DA"));
        EXPECT_TRUE(matching.is_universal_replacement_stable(
            system.get_agent_prefs(), system.get_firm_prefs(), system.get_agent_col_prefs(),
            system.get_firm_capacities()));
    }
}

TEST(ModifiedWorkerDA, RandomDeletionCanLeaveUniversalBlockingPair)
{
    const vector<vector<int>> agent_prefs = {{2, 1}, {2, 1}, {1, 2}, {1, 2}};
    const vector<vector<int>> firm_prefs = {{4, 3, 2, 1}, {4, 1, 2, 3}};
    vector<vector<int>> col_prefs(4, vector<int>(4, 0));
    col_prefs[0][1] = col_prefs[1][0] = -INF;
    col_prefs[1][2] = col_prefs[2][1] = -INF;
    bool saw_counterexample = false, saw_stable = false;
    for (unsigned int seed = 0; seed < 64; ++seed) {
        auto matching = modified_worker_proposing_DA_algorithm(
            4, 2, {2, 1}, agent_prefs, firm_prefs, col_prefs, seed);
        if (matching.get_agent_matchs() == vector<int>{1, UNMATCHED, 0, 0}) {
            saw_counterexample = true;
            EXPECT_FALSE(matching.is_universal_replacement_stable(
                agent_prefs, firm_prefs, col_prefs, {2, 1}));
            EXPECT_EQ(matching.universal_replacement_blocking_pairs(
                agent_prefs, firm_prefs, col_prefs, {2, 1}), (vector<pair<int, int>>{{0, 0}}));
        } else {
            saw_stable |= matching.is_universal_replacement_stable(
                agent_prefs, firm_prefs, col_prefs, {2, 1});
        }
    }
    EXPECT_TRUE(saw_counterexample);
    EXPECT_TRUE(saw_stable);
}

TEST(ModifiedWorkerDA, RandomMarketsReturnFeasibleConsistentRationalMatchings)
{
    mt19937 rng(42);
    uniform_int_distribution<int> score(-2, 3), capacity(0, 3), reject(0, 1);
    for (unsigned int seed = 0; seed < 300; ++seed) {
        SCOPED_TRACE(seed);
        vector<vector<int>> agent_prefs(5, vector<int>(3));
        vector<vector<int>> firm_prefs(3, vector<int>(5));
        vector<vector<int>> col_prefs(5, vector<int>(5, 0));
        vector<int> capacities(3);
        for (auto &row : agent_prefs) for (auto &entry : row) entry = score(rng);
        for (auto &row : firm_prefs) for (auto &entry : row) entry = score(rng);
        for (int left = 0; left < 5; ++left) {
            for (int right = 0; right < 5; ++right) {
                if (left != right && reject(rng)) col_prefs[left][right] = -INF;
            }
        }
        for (auto &entry : capacities) entry = capacity(rng);
        auto matching = modified_worker_proposing_DA_algorithm(
            5, 3, capacities, agent_prefs, firm_prefs, col_prefs, seed);
        vector<int> seen(5, 0);
        for (int firm = 0; firm < 3; ++firm) {
            const auto &workers = matching.get_firm_matchs()[firm];
            EXPECT_LE(static_cast<int>(workers.size()), capacities[firm]);
            for (int left : workers) {
                ++seen[left];
                EXPECT_EQ(matching.get_agent_matchs()[left], firm);
                EXPECT_GE(agent_prefs[left][firm], 0);
                EXPECT_GE(firm_prefs[firm][left], 0);
                for (int right : workers) if (left != right) EXPECT_NE(col_prefs[left][right], -INF);
            }
        }
        for (int worker = 0; worker < 5; ++worker) {
            EXPECT_EQ(seen[worker], matching.get_agent_matchs()[worker] == UNMATCHED ? 0 : 1);
        }
        EXPECT_TRUE(matching.is_individually_rational(agent_prefs, firm_prefs, col_prefs));
    }
}
