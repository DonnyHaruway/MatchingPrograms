#include <gtest/gtest.h>
#include "../../experiments/universal_replacement/rda_batch.hpp"

using namespace rda_batch;

TEST(RDABatch, GeneratorPreservesExactPairsLengthsAndPairedDomains)
{
    const std::vector<int> worker_lengths = {4, 3, 4, 4, 2, 4, 4, 2};
    const std::vector<int> firm_lengths = {7, 6, 7, 3, 4};
    for (std::uint64_t trial = 0; trial < 100; ++trial) {
        const auto complete = make_problem(20261021, trial, false);
        const auto incomplete = make_problem(20261021, trial, true);
        const auto repeat = make_problem(20261021, trial, true);
        EXPECT_EQ(incomplete.agent_prefs, repeat.agent_prefs);
        EXPECT_EQ(incomplete.firm_prefs, repeat.firm_prefs);
        EXPECT_EQ(incomplete.capacities, (std::vector<int>{2, 2, 2, 2, 1}));
        EXPECT_EQ(complete.colleague_prefs, incomplete.colleague_prefs);
        ASSERT_EQ(incomplete.forbidden_pairs.size(), 9u);
        std::size_t pairs = 0;
        for (int left = 0; left < 8; ++left) {
            EXPECT_EQ(incomplete.colleague_prefs[left][left], 0);
            for (int right = left + 1; right < 8; ++right) {
                EXPECT_EQ(incomplete.colleague_prefs[left][right], incomplete.colleague_prefs[right][left]);
                pairs += incomplete.colleague_prefs[left][right] == -MatchingTypes::INF;
            }
        }
        EXPECT_EQ(pairs, 9u);
        for (int worker = 0; worker < 8; ++worker) {
            EXPECT_EQ(complete.worker_rankings[worker].size(), 5u);
            EXPECT_EQ(incomplete.worker_rankings[worker].size(), worker_lengths[worker]);
            EXPECT_TRUE(std::equal(incomplete.worker_rankings[worker].begin(), incomplete.worker_rankings[worker].end(),
                                   complete.worker_rankings[worker].begin()));
        }
        for (int firm = 0; firm < 5; ++firm) {
            EXPECT_EQ(complete.firm_rankings[firm].size(), 8u);
            EXPECT_EQ(incomplete.firm_rankings[firm].size(), firm_lengths[firm]);
            EXPECT_TRUE(std::equal(incomplete.firm_rankings[firm].begin(), incomplete.firm_rankings[firm].end(),
                                   complete.firm_rankings[firm].begin()));
        }
    }
}

TEST(RDABatch, ScreenshotSummaryDistinguishesBothProposingSides)
{
    const auto problem = universal_replacement_experiment::screenshot_problem_5x8();
    for (bool firm : {true, false}) {
        const auto run = firm ? recursive_firm_proposing_DA_algorithm : recursive_doctor_proposing_DA_algorithm;
        const auto tree = run(8, 5, problem.capacities, problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs);
        const auto summary = summarize(tree, problem);
        EXPECT_EQ(summary.category(), firm ? "all_stable" : "none_stable");
        EXPECT_EQ(summary.nodes, firm ? 5u : 7u);
        EXPECT_EQ(summary.leaves, firm ? 3u : 4u);
        EXPECT_EQ(summary.stable_leaves, firm ? 3u : 0u);
        EXPECT_EQ(summary.initial_conflicts, 1u);
        EXPECT_EQ(summary.max_deletions, firm ? 2u : 3u);
        EXPECT_EQ(summary.stable_witness.has_value(), firm);
        EXPECT_EQ(summary.unstable_witness.has_value(), !firm);
        const auto leaf = firm ? *summary.stable_witness : *summary.unstable_witness;
        const auto path = history(tree, leaf);
        EXPECT_EQ(path.size(), tree.nodes[leaf].deletions.size());
        EXPECT_EQ(tree.branches[path.front()].from, 0u);
        EXPECT_EQ(tree.branches[path.back()].to, leaf);
    }
}

TEST(RDABatch, DuplicateBranchesAreIncludedInCounts)
{
    Problem problem;
    problem.capacities = {3};
    problem.agent_prefs = {{1}, {1}, {1}};
    problem.firm_prefs = {{3, 2, 1}};
    problem.colleague_prefs = {{0, -MatchingTypes::INF, -MatchingTypes::INF}, {0, 0, 0}, {0, 0, 0}};
    const auto tree = recursive_firm_proposing_DA_algorithm(3, 1, problem.capacities,
        problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs);
    const auto summary = summarize(tree, problem);
    EXPECT_EQ(summary.nodes, 9u);
    EXPECT_EQ(summary.branches, 8u);
    EXPECT_EQ(summary.leaves, 6u);
    EXPECT_EQ(summary.distinct_matchings, 4u);
}

TEST(RDABatch, MixedCategoryAndTrivialRootAreNotConflated)
{
    Problem problem;
    problem.capacities = {2, 1};
    problem.agent_prefs = universal_replacement_experiment::ranking_scores({{2, 1}, {1}, {1}, {1, 2}}, 2);
    problem.firm_prefs = universal_replacement_experiment::ranking_scores({{1, 2, 3, 4}, {4, 1}}, 4);
    problem.colleague_prefs.assign(4, std::vector<int>(4, 0));
    problem.colleague_prefs[0][1] = problem.colleague_prefs[0][2] = -MatchingTypes::INF;
    auto tree = recursive_firm_proposing_DA_algorithm(4, 2, problem.capacities,
        problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs);
    auto result = summarize(tree, problem);
    EXPECT_EQ(result.category(), "mixed");
    EXPECT_EQ(result.leaves, 3u);
    EXPECT_EQ(result.stable_leaves, 1u);
    problem.colleague_prefs.assign(4, std::vector<int>(4, 0));
    tree = recursive_firm_proposing_DA_algorithm(4, 2, problem.capacities,
        problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs);
    result = summarize(tree, problem);
    EXPECT_EQ(result.category(), "all_stable");
    EXPECT_EQ(result.initial_conflicts, 0u);
    EXPECT_EQ(result.nodes, 1u);
    EXPECT_TRUE(history(tree, *result.stable_witness).empty());
}
