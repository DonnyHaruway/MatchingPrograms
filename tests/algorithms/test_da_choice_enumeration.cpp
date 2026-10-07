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

TEST(DAChoiceEnumeration, ScreenshotHypothesisIsFalseOnExhaustivePaths)
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
        // Independent prototype using a temporary copy of the original C++
        // algorithm previously established these complete path counts.
        EXPECT_EQ(stats.completed_paths, firm ? 3u : 10u);
        EXPECT_EQ(stable_paths, firm ? 3u : 8u);
        EXPECT_EQ(outcomes.size(), firm ? 3u : 5u);
        EXPECT_EQ(stable_outcomes.size(), firm ? 3u : 4u);
        EXPECT_EQ(stats.max_simultaneous_conflicts, 1u);
    }
}
