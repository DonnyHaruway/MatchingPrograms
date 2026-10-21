#include <gtest/gtest.h>
#include "MatchingSystem.hpp"

using namespace std;
using namespace MatchingTypes;

class IteratedDAPolicies : public testing::TestWithParam<bool> {
protected:
    Matching run(
        bool history,
        const vector<int> &capacities,
        const vector<vector<int>> &agents,
        const vector<vector<int>> &firms,
        const vector<vector<int>> &colleagues,
        unsigned int seed = 42,
        const DARandomChoiceSelector &selector = {}
    ) {
        auto algorithm = history
            ? (GetParam() ? iterated_firm_proposing_DA_history_priority_algorithm
                          : iterated_doctor_proposing_DA_history_priority_algorithm)
            : (GetParam() ? iterated_firm_proposing_DA_firm_preference_algorithm
                          : iterated_doctor_proposing_DA_firm_preference_algorithm);
        return algorithm(static_cast<int>(agents.size()), static_cast<int>(firms.size()),
                         capacities, agents, firms, colleagues, seed, selector);
    }

    size_t choose_pair(const DARandomChoice &choice, int firm, int left, int right) {
        for (size_t index = 0; index < choice.conflicts.size(); ++index) {
            const auto &pair = choice.conflicts[index];
            if (pair.firm == firm && pair.left == left && pair.right == right) return index;
        }
        ADD_FAILURE() << "Requested pair not found in round " << choice.round;
        return choice.option_count;
    }
};

TEST_P(IteratedDAPolicies, HistoryRandomizesBothNewEndpointsInEitherRejectionDirection)
{
    for (const auto &colleagues : vector<vector<vector<int>>>{
             {{0, -INF}, {0, 0}}, {{0, 0}, {-INF, 0}}}) {
        for (size_t endpoint : {0u, 1u}) {
            int decisions = 0;
            auto matching = run(true, {2}, {{1}, {1}}, {{2, 1}}, colleagues, 42,
                [&](const DARandomChoice &choice) {
                    EXPECT_EQ(choice.kind, DAChoiceKind::removal_endpoint);
                    EXPECT_EQ(choice.option_count, 2u);
                    ++decisions;
                    return endpoint;
                });
            EXPECT_EQ(decisions, 1);
            EXPECT_EQ(matching.get_agent_matchs()[endpoint], UNMATCHED);
            EXPECT_EQ(matching.get_agent_matchs()[1 - endpoint], 0);
        }
    }
}

TEST_P(IteratedDAPolicies, HistoryRetainsPreviouslyKeptRightEndpointRegardlessOfFirmScore)
{
    vector<vector<int>> colleagues(3, vector<int>(3, 0));
    colleagues[0][2] = colleagues[1][2] = -INF;
    int endpoint_decisions = 0;
    auto matching = run(true, {3}, {{1}, {1}, {1}}, {{3, 2, 1}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            if (choice.kind == DAChoiceKind::pair_order) return choose_pair(choice, 0, 1, 2);
            EXPECT_EQ(choice.round, 1);
            ++endpoint_decisions;
            return 0; // Keep worker 2, despite the firm's lower score for that worker.
        });
    EXPECT_EQ(endpoint_decisions, 1);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{UNMATCHED, UNMATCHED, 0}));
}

TEST_P(IteratedDAPolicies, HistoryRetainsOlderSurvivorWithoutOverwritingFirstRound)
{
    vector<vector<int>> colleagues(5, vector<int>(5, 0));
    colleagues[0][1] = colleagues[0][2] = colleagues[0][3] = colleagues[3][4] = -INF;
    int endpoint_decisions = 0;
    auto matching = run(true, {5}, {{1}, {1}, {1}, {1}, {1}}, {{1, 2, 3, 4, 5}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            if (choice.kind == DAChoiceKind::pair_order) {
                if (choice.round == 1) return choose_pair(choice, 0, 0, 1);
                if (choice.round == 2) return choose_pair(choice, 0, 3, 4);
                EXPECT_EQ(choice.round, 3);
                return choose_pair(choice, 0, 0, 2);
            }
            EXPECT_LE(choice.round, 2);
            ++endpoint_decisions;
            return 1; // Initially retain 0 in round 1 and 3 in round 2.
        });
    // Retaining 0 again in round 3 must not make its history newer than worker 3's.
    EXPECT_EQ(endpoint_decisions, 2);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{0, UNMATCHED, UNMATCHED, UNMATCHED, UNMATCHED}));
}

TEST_P(IteratedDAPolicies, HistoryBelongsToFirmWorkerPairAndAllowsNewRandomDecisionAfterMove)
{
    vector<vector<int>> colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[2][3] = colleagues[0][2] = colleagues[1][2] = -INF;
    vector<int> random_rounds;
    auto matching = run(true, {4, 4}, {{2, 1}, {2, 1}, {2, 1}, {2, 1}},
                       {{4, 3, 2, 1}, {4, 3, 2, 1}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            if (choice.kind == DAChoiceKind::pair_order) {
                if (choice.round == 1) return choose_pair(choice, 0, 0, 1);
                if (choice.round == 2) return choose_pair(choice, 0, 2, 3);
                EXPECT_EQ(choice.round, 4);
                return choose_pair(choice, 1, 1, 2);
            }
            random_rounds.push_back(choice.round);
            return 1;
        });
    // Worker 2 was retained at firm 0, then removed there in round 3. At firm 1
    // neither worker has history, so the selector must be allowed to remove 2.
    EXPECT_EQ(random_rounds, (vector<int>{1, 2, 4}));
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{0, 1, UNMATCHED, 1}));
}

TEST_P(IteratedDAPolicies, HistoryRemovesNewerLeftEndpointWhenBothHaveHistory)
{
    vector<vector<int>> colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[2][3] = colleagues[0][2] = -INF;
    int endpoint_decisions = 0;
    auto matching = run(true, {4}, {{1}, {1}, {1}, {1}}, {{4, 3, 2, 1}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            if (choice.kind == DAChoiceKind::pair_order) {
                return choice.round == 1 ? choose_pair(choice, 0, 2, 3)
                                         : choose_pair(choice, 0, 0, 1);
            }
            EXPECT_LE(choice.round, 2);
            ++endpoint_decisions;
            return 1;
        });
    EXPECT_EQ(endpoint_decisions, 2);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{UNMATCHED, UNMATCHED, 0, UNMATCHED}));
}

TEST_P(IteratedDAPolicies, FirmPreferenceRemovesLowerScoreAndBreaksTiesByWorkerId)
{
    for (const auto &scores : vector<vector<int>>{{1, 2}, {2, 1}, {1, 1}}) {
        auto matching = run(false, {2}, {{1}, {1}}, {scores}, {{0, -INF}, {0, 0}}, 42,
            [](const DARandomChoice &) -> size_t {
                ADD_FAILURE() << "A single pair needs no random choice under firm preference";
                return 0;
            });
        const int retained = scores[0] < scores[1] ? 1 : 0;
        EXPECT_EQ(matching.get_agent_matchs()[retained], 0);
        EXPECT_EQ(matching.get_agent_matchs()[1 - retained], UNMATCHED);
    }
}

TEST_P(IteratedDAPolicies, FirmPreferenceChoosesOnePairAcrossFirmsAndRestartsBeforeNextDeletion)
{
    vector<vector<int>> colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[2][3] = -INF;
    int pair_decisions = 0;
    auto matching = run(false, {2, 2}, {{2, -1}, {2, -1}, {-1, 2}, {-1, 2}},
                       {{4, 3, -1, -1}, {-1, -1, 1, 2}}, colleagues, 42,
        [&](const DARandomChoice &choice) -> size_t {
            EXPECT_EQ(choice.kind, DAChoiceKind::pair_order);
            EXPECT_EQ(choice.round, 1);
            EXPECT_EQ(choice.agent_match, (vector<int>{0, 0, 1, 1}));
            ++pair_decisions;
            return choose_pair(choice, 1, 2, 3);
        });
    EXPECT_EQ(pair_decisions, 1);
    EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{0, UNMATCHED, UNMATCHED, 1}));
}

TEST_P(IteratedDAPolicies, BothPoliciesUseProposingSideAndFinishDABeforeCheckingConflicts)
{
    for (bool history : {false, true}) {
        const auto no_choice = [](const DARandomChoice &) -> size_t {
            ADD_FAILURE() << "No conflict remains after complete DA";
            return 0;
        };
        const auto matching = run(history, {1, 1}, {{2, 1}, {1, 2}}, {{1, 2}, {2, 1}},
                                  {{0, 0}, {0, 0}}, 42, no_choice);
        EXPECT_EQ(matching.get_agent_matchs(), GetParam() ? (vector<int>{1, 0}) : (vector<int>{0, 1}));

        vector<vector<int>> colleagues(3, vector<int>(3, 0));
        colleagues[0][1] = -INF;
        auto finished = GetParam()
            ? run(history, {2, 1}, {{1, 2}, {2, 1}, {2, 1}}, {{3, 2, 1}, {3, 1, 2}}, colleagues, 42, no_choice)
            : run(history, {2}, {{1}, {1}, {1}}, {{3, 1, 2}}, colleagues, 42, no_choice);
        EXPECT_EQ(finished.get_agent_matchs(), GetParam() ? (vector<int>{1, 0, 0}) : (vector<int>{0, UNMATCHED, 0}));
    }
}

TEST_P(IteratedDAPolicies, BothPoliciesRefillVacancyWithPreviouslyRejectedWorker)
{
    for (bool history : {false, true}) {
        auto matching = run(history, {2}, {{1}, {1}, {1}}, {{3, 2, 1}},
                            {{0, -INF, 0}, {0, 0, 0}, {0, 0, 0}}, 42,
                            [](const DARandomChoice &) -> size_t { return 1; });
        EXPECT_EQ(matching.get_agent_matchs(), (vector<int>{0, UNMATCHED, 0}));
    }
}

TEST_P(IteratedDAPolicies, BothPoliciesDispatchReproduciblyAndPreserveInputs)
{
    const vector<vector<int>> agents = {{2, 1}, {2, 1}, {1, 2}, {1, 2}};
    const vector<vector<int>> firms = {{4, 3, 2, 1}, {4, 1, 2, 3}};
    vector<vector<int>> colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[1][2] = -INF;
    MatchingSystem system(4, 2);
    system.set_prefs(agents, firms, colleagues);
    system.set_firm_capacities({2, 1});
    for (bool history : {false, true}) {
        const string name = string(GetParam() ? "iterated_firm_propose_DA_" : "iterated_doctor_propose_DA_")
            + (history ? "history_priority" : "firm_preference");
        auto matching = system.run_algorithm(name, 42);
        EXPECT_EQ(matching, system.run_algorithm(name, 42));
        EXPECT_EQ(matching, run(history, {2, 1}, agents, firms, colleagues));
        EXPECT_EQ(system.get_agent_prefs(), agents);
        EXPECT_EQ(system.get_firm_prefs(), firms);
        EXPECT_EQ(system.get_agent_col_prefs(), colleagues);
        EXPECT_NO_THROW(matching.get_agent_scores());
        EXPECT_NO_THROW(matching.get_firm_scores());
    }
}

TEST_P(IteratedDAPolicies, BothPoliciesRejectInvalidRandomChoices)
{
    const auto invalid = [](const DARandomChoice &choice) { return choice.option_count; };
    EXPECT_THROW(run(true, {2}, {{1}, {1}}, {{2, 1}}, {{0, -INF}, {0, 0}}, 42, invalid), out_of_range);
    vector<vector<int>> colleagues(3, vector<int>(3, -INF));
    for (int worker = 0; worker < 3; ++worker) colleagues[worker][worker] = 0;
    for (bool history : {false, true}) {
        EXPECT_THROW(run(history, {3}, {{1}, {1}, {1}}, {{3, 2, 1}}, colleagues, 42, invalid), out_of_range);
    }
}

TEST_P(IteratedDAPolicies, BothPoliciesHandleEmptyMarketsAndUnacceptablePreferences)
{
    for (bool history : {false, true}) {
        EXPECT_TRUE(run(history, {1, 1}, {}, {{}, {}}, {}).get_agent_matchs().empty());
        EXPECT_EQ(run(history, {}, {{}, {}}, {}, {{0, 0}, {0, 0}}).get_agent_matchs(), (vector<int>{UNMATCHED, UNMATCHED}));
        EXPECT_EQ(run(history, {0}, {{1}}, {{1}}, {{0}}).get_agent_matchs(), (vector<int>{UNMATCHED}));
        EXPECT_EQ(run(history, {1}, {{-1}}, {{1}}, {{0}}).get_agent_matchs(), (vector<int>{UNMATCHED}));
        EXPECT_EQ(run(history, {1}, {{1}}, {{-1}}, {{0}}).get_agent_matchs(), (vector<int>{UNMATCHED}));
        EXPECT_EQ(run(history, {1}, {{0}}, {{0}}, {{0}}).get_agent_matchs(), (vector<int>{0}));
    }
}

TEST_P(IteratedDAPolicies, RandomMarketsAreFeasibleAndHaveNoColleagueRejection)
{
    mt19937 rng(42);
    uniform_int_distribution<int> score(-2, 3), capacity(0, 3), reject(0, 1);
    for (unsigned int seed = 0; seed < 100; ++seed) {
        SCOPED_TRACE(seed);
        vector<vector<int>> agents(5, vector<int>(3)), firms(3, vector<int>(5));
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
        for (bool history : {false, true}) {
            const auto matching = run(history, capacities, agents, firms, colleagues, seed);
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
}

INSTANTIATE_TEST_SUITE_P(ProposingSides, IteratedDAPolicies, testing::Bool(),
    [](const testing::TestParamInfo<bool> &info) { return info.param ? "Firm" : "Doctor"; });
