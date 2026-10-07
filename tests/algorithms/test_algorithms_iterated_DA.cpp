#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
using namespace MatchingTypes;

class IteratedDA : public testing::TestWithParam<bool> {
protected:
    Matching run(
        const vector<int> &capacities,
        const vector<vector<int>> &agents,
        const vector<vector<int>> &firms,
        const vector<vector<int>> &colleagues,
        unsigned int seed = 42,
        const DARandomChoiceSelector &selector = {}
    ) {
        auto algorithm = GetParam() ? iterated_firm_proposing_DA_algorithm
                                    : iterated_doctor_proposing_DA_algorithm;
        return algorithm(static_cast<int>(agents.size()), static_cast<int>(firms.size()),
                         capacities, agents, firms, colleagues, seed, selector);
    }
};

TEST_P(IteratedDA, NoConflictsUsesCorrectProposingSideAndStopsWithoutDeletion)
{
    const vector<vector<int>> agents = {{2, 1}, {1, 2}};
    const vector<vector<int>> firms = {{1, 2}, {2, 1}};
    const auto matching = run({1, 1}, agents, firms, {{0, 0}, {0, 0}}, 42,
        [](const DARandomChoice &) -> size_t {
            ADD_FAILURE() << "No random decision is needed without a conflict";
            return 0;
        });
    EXPECT_EQ(matching.get_agent_matchs(), GetParam() ? (vector<int>{1, 0}) : (vector<int>{0, 1}));
}

TEST_P(IteratedDA, RestartRefillsVacancyWithPreviouslyRejectedAgent)
{
    vector<vector<int>> colleagues(3, vector<int>(3, 0));
    colleagues[0][1] = -INF;
    int deletions = 0;
    auto matching = run({2}, {{1}, {1}, {1}}, {{3, 2, 1}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            EXPECT_EQ(choice.kind, DAChoiceKind::removal_endpoint);
            EXPECT_EQ(choice.round, 1);
            EXPECT_EQ(choice.agent_match, (vector<int>{0, 0, UNMATCHED}));
            ++deletions;
            return 0; // Remove agent 0 from firm 0.
        });
    EXPECT_EQ(deletions, 1);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{UNMATCHED, 0, 0}));
}

TEST_P(IteratedDA, DeletionOnlyAffectsTheMatchedFirm)
{
    auto matching = run({2, 2}, {{2, 1}, {2, 1}}, {{2, 1}, {2, 1}},
                        {{0, -INF}, {0, 0}}, 42,
                        [](const DARandomChoice &) -> size_t { return 0; });
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{1, 0}));
}

TEST_P(IteratedDA, CumulativeDeletionsSurviveEveryRestart)
{
    vector<vector<int>> colleagues(4, vector<int>(4, -INF));
    for (int agent = 0; agent < 4; ++agent) colleagues[agent][agent] = 0;
    int deletions = 0;
    auto matching = run({2}, {{1}, {1}, {1}, {1}}, {{4, 3, 2, 1}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            EXPECT_EQ(choice.kind, DAChoiceKind::removal_endpoint);
            EXPECT_EQ(choice.conflicts.size(), 1u);
            const auto pair = choice.conflicts.front();
            EXPECT_EQ(choice.round, deletions + 1);
            EXPECT_EQ(pair.left, deletions);
            EXPECT_EQ(pair.right, deletions + 1);
            vector<int> expected(4, UNMATCHED);
            expected[deletions] = expected[deletions + 1] = 0;
            EXPECT_EQ(choice.agent_match, expected);
            ++deletions;
            return 0;
        });
    EXPECT_EQ(deletions, 3);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{UNMATCHED, UNMATCHED, UNMATCHED, 0}));
}

TEST_P(IteratedDA, DeletesOnlyOneEndpointBeforeRestart)
{
    vector<vector<int>> colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[2][3] = -INF;
    int pair_choices = 0, deletions = 0;
    auto matching = run({4}, {{1}, {1}, {1}, {1}}, {{4, 3, 2, 1}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            if (choice.kind == DAChoiceKind::pair_order) {
                EXPECT_EQ(choice.round, 1);
                EXPECT_EQ(choice.option_count, 2u);
                ++pair_choices;
                return 1; // Choose (2, 3) before (0, 1).
            }
            ++deletions;
            EXPECT_EQ(choice.round, deletions);
            EXPECT_EQ(choice.conflicts.front().left, deletions == 1 ? 2 : 0);
            EXPECT_EQ(choice.agent_match, deletions == 1
                ? (vector<int>{0, 0, 0, 0}) : (vector<int>{0, 0, UNMATCHED, 0}));
            return 0;
        });
    EXPECT_EQ(pair_choices, 1);
    EXPECT_EQ(deletions, 2);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{UNMATCHED, 0, UNMATCHED, 0}));
}

TEST_P(IteratedDA, ChoosesPairsAcrossAllFirms)
{
    vector<vector<int>> colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[2][3] = -INF;
    vector<int> deletion_firms;
    auto matching = run({2, 2}, {{2, -1}, {2, -1}, {-1, 2}, {-1, 2}},
                       {{4, 3, -1, -1}, {-1, -1, 2, 1}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            if (choice.kind == DAChoiceKind::pair_order) {
                EXPECT_EQ(choice.conflicts.size(), 2u);
                EXPECT_EQ(choice.conflicts[0].firm, 0);
                EXPECT_EQ(choice.conflicts[1].firm, 1);
                return 1;
            }
            deletion_firms.push_back(choice.conflicts.front().firm);
            return 0;
        });
    EXPECT_EQ(deletion_firms, (vector<int>{1, 0}));
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{UNMATCHED, 0, UNMATCHED, 1}));
}

TEST_P(IteratedDA, RejectionInEitherDirectionAndBothEndpointsAreSupported)
{
    for (const auto &colleagues : vector<vector<vector<int>>>{
             {{0, -INF}, {0, 0}}, {{0, 0}, {-INF, 0}}}) {
        for (size_t endpoint : {0u, 1u}) {
            auto matching = run({2}, {{1}, {1}}, {{2, 1}}, colleagues, 42,
                [endpoint](const DARandomChoice &) { return endpoint; });
            EXPECT_EQ(matching.get_agent_matchs()[endpoint], UNMATCHED);
            EXPECT_EQ(matching.get_agent_matchs()[1 - endpoint], 0);
        }
    }
}

TEST_P(IteratedDA, NegativePreferencesZeroCapacityAndZeroScores)
{
    auto matching = run({0, 2}, {{2, 1}, {-1, 0}, {1, 1}}, {{3, 2, 1}, {2, 0, -1}},
                        vector<vector<int>>(3, vector<int>(3, 0)));
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{1, 1, UNMATCHED}));
    EXPECT_EQ(run({1}, {{-1}}, {{1}}, {{0}}).get_agent_matchs(), (vector<int>{UNMATCHED}));
    EXPECT_EQ(run({1}, {{0}}, {{0}}, {{0}}).get_agent_matchs(), (vector<int>{0}));
}

TEST_P(IteratedDA, EmptyMarketsTerminate)
{
    auto no_agents = run({1, 1}, {}, {{}, {}}, {});
    EXPECT_TRUE(no_agents.get_agent_matchs().empty());
    EXPECT_EQ(no_agents.get_firm_matchs(), (vector<set<int>>(2)));
    auto no_firms = run({}, {{}, {}}, {}, {{0, 0}, {0, 0}});
    EXPECT_EQ(no_firms.get_agent_matchs(), (vector<int>{UNMATCHED, UNMATCHED}));
}

TEST_P(IteratedDA, SeededDispatchIsReproducibleAndPreservesInputs)
{
    const vector<vector<int>> agents = {{2, 1}, {2, 1}, {1, 2}, {1, 2}};
    const vector<vector<int>> firms = {{4, 3, 2, 1}, {4, 1, 2, 3}};
    vector<vector<int>> colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[1][2] = -INF;
    MatchingSystem system(4, 2);
    system.set_prefs(agents, firms, colleagues);
    system.set_firm_capacities({2, 1});
    const string name = GetParam() ? "iterated_firm_propose_DA" : "iterated_doctor_propose_DA";
    auto matching = system.run_algorithm(name, 42);
    EXPECT_EQ(matching, system.run_algorithm(name, 42));
    EXPECT_EQ(matching, run({2, 1}, agents, firms, colleagues));
    EXPECT_EQ(system.get_agent_prefs(), agents);
    EXPECT_EQ(system.get_firm_prefs(), firms);
    EXPECT_EQ(system.get_agent_col_prefs(), colleagues);
    EXPECT_NO_THROW(matching.get_agent_scores());
    EXPECT_NO_THROW(matching.get_firm_scores());
}

TEST_P(IteratedDA, RejectsOutOfRangeChoiceIndices)
{
    vector<vector<int>> colleagues(3, vector<int>(3, -INF));
    for (int agent = 0; agent < 3; ++agent) colleagues[agent][agent] = 0;
    EXPECT_THROW(run({3}, {{1}, {1}, {1}}, {{3, 2, 1}}, colleagues, 42,
        [](const DARandomChoice &choice) { return choice.option_count; }), out_of_range);
    EXPECT_THROW(run({2}, {{1}, {1}}, {{2, 1}}, {{0, -INF}, {0, 0}}, 42,
        [](const DARandomChoice &choice) { return choice.option_count; }), out_of_range);
}

TEST_P(IteratedDA, RandomMarketsAreFeasibleAndHaveNoColleagueRejection)
{
    mt19937 rng(42);
    uniform_int_distribution<int> score(-2, 3), capacity(0, 3), reject(0, 1);
    for (unsigned int seed = 0; seed < 200; ++seed) {
        SCOPED_TRACE(seed);
        vector<vector<int>> agents(5, vector<int>(3));
        vector<vector<int>> firms(3, vector<int>(5));
        vector<vector<int>> colleagues(5, vector<int>(5, 0));
        vector<int> capacities(3);
        for (auto &row : agents) for (int &value : row) value = score(rng);
        for (auto &row : firms) for (int &value : row) value = score(rng);
        for (int left = 0; left < 5; ++left) {
            for (int right = 0; right < 5; ++right) {
                if (left != right && reject(rng)) colleagues[left][right] = -INF;
            }
        }
        for (int &value : capacities) value = capacity(rng);
        auto matching = run(capacities, agents, firms, colleagues, seed);
        for (int firm = 0; firm < 3; ++firm) {
            const auto &workers = matching.get_firm_matchs()[firm];
            EXPECT_LE(workers.size(), static_cast<size_t>(capacities[firm]));
            for (int worker : workers) {
                EXPECT_EQ(matching.get_agent_matchs()[worker], firm);
                EXPECT_GE(agents[worker][firm], 0);
                EXPECT_GE(firms[firm][worker], 0);
                for (int colleague : workers) EXPECT_NE(colleagues[worker][colleague], -INF);
            }
        }
        for (int worker = 0; worker < 5; ++worker) {
            const int firm = matching.get_agent_matchs()[worker];
            if (firm != UNMATCHED) EXPECT_EQ(matching.get_firm_matchs()[firm].count(worker), 1u);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(ProposingSides, IteratedDA, testing::Bool(),
    [](const testing::TestParamInfo<bool> &info) { return info.param ? "Firm" : "Doctor"; });

TEST(IteratedDAOrder, DoctorDAFinishesBeforeDetectingConflicts)
{
    vector<vector<int>> colleagues(3, vector<int>(3, 0));
    colleagues[0][1] = -INF;
    auto matching = iterated_doctor_proposing_DA_algorithm(
        3, 1, {2}, {{1}, {1}, {1}}, {{3, 1, 2}}, colleagues, 42,
        [](const DARandomChoice &) -> size_t {
            ADD_FAILURE() << "The conflicting provisional pair has already been displaced";
            return 0;
        });
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{0, UNMATCHED, 0}));
}

TEST(IteratedDAOrder, FirmDAFinishesBeforeDetectingConflicts)
{
    vector<vector<int>> colleagues(3, vector<int>(3, 0));
    colleagues[0][1] = -INF;
    auto matching = iterated_firm_proposing_DA_algorithm(
        3, 2, {2, 1}, {{1, 2}, {2, 1}, {2, 1}}, {{3, 2, 1}, {3, 1, 2}}, colleagues, 42,
        [](const DARandomChoice &) -> size_t {
            ADD_FAILURE() << "Agent 0 must move to firm 1 before checking conflicts";
            return 0;
        });
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{1, 0, 0}));
}
