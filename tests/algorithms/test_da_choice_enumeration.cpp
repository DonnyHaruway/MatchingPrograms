#include <gtest/gtest.h>
#include "algorithms.hpp"
#include "../../experiments/universal_replacement/choice_search.hpp"
#include "../../experiments/universal_replacement/problem_5x8.hpp"

using namespace std;
using namespace universal_replacement_experiment;

TEST(DAChoiceEnumeration, EnumeratesPairOrdersAndSkipsResolvedPairs)
{
    vector<vector<int>> colleagues(3, vector<int>(3, 0));
    colleagues[0][2] = colleagues[2][0] = -MatchingTypes::INF;
    colleagues[1][2] = colleagues[2][1] = -MatchingTypes::INF;
    set<vector<int>> outcomes;
    const auto stats = enumerate_choice_paths(
        [&](const DARandomChoiceSelector &selector) {
            return modified_worker_proposing_DA_algorithm(
                3, 1, {3}, {{1}, {1}, {1}}, {{3, 2, 1}}, colleagues, 0, selector);
        },
        [&](uint64_t, Matching &matching, const vector<ChoiceDecision> &decisions) {
            outcomes.insert(matching.get_agent_matchs());
            ASSERT_EQ(decisions.front().choice.kind, DAChoiceKind::pair_order);
            EXPECT_EQ(decisions.front().choice.option_count, 2u);
            // Removing agent 2 on the first pair resolves both pairs, so there
            // must be no redundant endpoint decision for the second pair.
            const auto &first = decisions[1];
            const auto pair = first.choice.conflicts.front();
            const int removed = first.selected == 0 ? pair.left : pair.right;
            EXPECT_EQ(decisions.size(), removed == 2 ? 2u : 3u);
        });
    EXPECT_EQ(stats.completed_paths, 6u); // Two orders, each with 3 endpoint histories.
    EXPECT_EQ(stats.max_simultaneous_conflicts, 2u);
    EXPECT_EQ(outcomes.size(), 4u);
}

TEST(DAChoiceEnumeration, RejectsInvalidSelectorChoices)
{
    const vector<vector<int>> colleagues = {{0, -MatchingTypes::INF}, {-MatchingTypes::INF, 0}};
    EXPECT_THROW(modified_worker_proposing_DA_algorithm(
        2, 1, {2}, {{1}, {1}}, {{2, 1}}, colleagues, 0,
        [](const DARandomChoice &choice) { return choice.option_count; }), out_of_range);
}

TEST(DAChoiceEnumeration, ScreenshotWithVacancyBlockingHasOnlyFirmStablePaths)
{
    const auto problem = screenshot_problem_5x8();
    for (bool firm : {true, false}) {
        uint64_t stable_paths = 0;
        set<vector<int>> outcomes;
        set<vector<int>> stable_outcomes;
        const auto stats = enumerate_choice_paths(
            [&](const DARandomChoiceSelector &selector) {
                return firm
                    ? modified_firm_proposing_DA_algorithm(8, 5, problem.capacities,
                        problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, 0, selector)
                    : modified_worker_proposing_DA_algorithm(8, 5, problem.capacities,
                        problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, 0, selector);
            },
            [&](uint64_t, Matching &matching, const vector<ChoiceDecision> &) {
                outcomes.insert(matching.get_agent_matchs());
                if (matching.is_universal_replacement_stable(problem.agent_prefs, problem.firm_prefs,
                                                             problem.colleague_prefs, problem.capacities)) {
                    ++stable_paths;
                    stable_outcomes.insert(matching.get_agent_matchs());
                }
            });
        // Independent Python verifier checks vacancy additions and universal
        // replacements against every final C++ assignment on all paths.
        EXPECT_EQ(stats.completed_paths, firm ? 3u : 10u);
        EXPECT_EQ(stable_paths, firm ? 2u : 0u);
        EXPECT_EQ(outcomes.size(), firm ? 3u : 5u);
        EXPECT_EQ(stable_outcomes.size(), firm ? 2u : 0u);
        EXPECT_EQ(stats.max_simultaneous_conflicts, 1u);
    }
}

TEST(DAChoiceEnumeration, IteratedDoctorScreenshotHasNoUniversalStablePath)
{
    const auto problem = screenshot_problem_5x8();
    set<vector<int>> outcomes;
    set<pair<int, int>> observed_blockers;
    uint64_t stable_paths = 0;
    const auto stats = enumerate_choice_paths(
        [&](const DARandomChoiceSelector &selector) {
            return iterated_doctor_proposing_DA_algorithm(
                8, 5, problem.capacities, problem.agent_prefs,
                problem.firm_prefs, problem.colleague_prefs, 0, selector);
        },
        [&](uint64_t, Matching &matching, const vector<ChoiceDecision> &decisions) {
            outcomes.insert(matching.get_agent_matchs());
            EXPECT_TRUE(matching.is_individually_rational(
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs));
            stable_paths += matching.is_universal_replacement_stable(
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, problem.capacities);
            const auto blockers = matching.universal_replacement_blocking_pairs(
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, problem.capacities);
            ASSERT_EQ(blockers.size(), 1u);
            observed_blockers.insert(blockers.front());
            for (size_t index = 0; index < decisions.size(); ++index) {
                EXPECT_EQ(decisions[index].choice.kind, DAChoiceKind::removal_endpoint);
                EXPECT_EQ(decisions[index].choice.round, static_cast<int>(index + 1));
            }
        });
    // Independent Python enumeration verifies these complete-DA restart paths.
    EXPECT_EQ(stats.completed_paths, 4u);
    EXPECT_EQ(stats.replay_calls, 7u);
    EXPECT_EQ(stats.max_decisions, 3u);
    EXPECT_EQ(stats.max_simultaneous_conflicts, 1u);
    EXPECT_EQ(stable_paths, 0u);
    EXPECT_EQ(outcomes, (set<vector<int>>{
        {0, 2, 1, 2, -1, 1, 0, 4},
        {0, 1, 2, 3, 0, 2, 1, 4},
        {0, 0, 2, 1, -1, 2, 1, 4},
        {0, 1, 2, 2, 0, 3, 1, 4}}));
    // Matching returns (worker, firm), with zero-based IDs.
    EXPECT_EQ(observed_blockers, (set<pair<int, int>>{{1, 0}, {3, 2}, {1, 1}, {5, 0}}));
}

TEST(DAChoiceEnumeration, IteratedFirmScreenshotIsUniversalStableOnEveryPath)
{
    const auto problem = screenshot_problem_5x8();
    set<vector<int>> outcomes;
    uint64_t stable_paths = 0, fully_matched_paths = 0;
    const auto stats = enumerate_choice_paths(
        [&](const DARandomChoiceSelector &selector) {
            return iterated_firm_proposing_DA_algorithm(
                8, 5, problem.capacities, problem.agent_prefs,
                problem.firm_prefs, problem.colleague_prefs, 0, selector);
        },
        [&](uint64_t, Matching &matching, const vector<ChoiceDecision> &decisions) {
            const auto &assignment = matching.get_agent_matchs();
            outcomes.insert(assignment);
            EXPECT_TRUE(matching.is_individually_rational(
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs));
            stable_paths += matching.is_universal_replacement_stable(
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, problem.capacities);
            EXPECT_TRUE(matching.universal_replacement_blocking_pairs(
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, problem.capacities).empty());
            fully_matched_paths += find(assignment.begin(), assignment.end(), MatchingTypes::UNMATCHED) == assignment.end();
            for (size_t index = 0; index < decisions.size(); ++index) {
                EXPECT_EQ(decisions[index].choice.kind, DAChoiceKind::removal_endpoint);
                EXPECT_EQ(decisions[index].choice.round, static_cast<int>(index + 1));
            }
        });
    // Independent Python firm DA verifies all three restart paths and endpoints.
    EXPECT_EQ(stats.completed_paths, 3u);
    EXPECT_EQ(stats.replay_calls, 5u);
    EXPECT_EQ(stats.max_decisions, 2u);
    EXPECT_EQ(stats.max_simultaneous_conflicts, 1u);
    EXPECT_EQ(stable_paths, 3u);
    EXPECT_EQ(fully_matched_paths, 1u);
    EXPECT_EQ(outcomes, (set<vector<int>>{
        {1, 2, 1, 2, -1, 0, 0, 4},
        {2, 0, 1, 3, 2, 1, 0, 4},
        {2, 0, 1, 2, -1, 1, 0, 4}}));
}
