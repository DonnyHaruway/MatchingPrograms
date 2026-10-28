#include <gtest/gtest.h>
#include "MatchingSystem.hpp"
#include "../../experiments/universal_replacement/choice_search.hpp"
#include "../../experiments/universal_replacement/problem_5x8.hpp"

using namespace std;
using namespace MatchingTypes;
using namespace universal_replacement_experiment;

class RecursiveDA : public testing::TestWithParam<bool> {
protected:
    DASearchTree run(const vector<int> &capacities, const Rankings &agents,
                     const Rankings &firms, const Rankings &colleagues) {
        auto algorithm = GetParam() ? recursive_firm_proposing_DA_algorithm
                                    : recursive_doctor_proposing_DA_algorithm;
        return algorithm(agents.size(), firms.size(), capacities, agents, firms, colleagues);
    }
};

TEST_P(RecursiveDA, NoConflictsUsesCorrectProposingSideAndReturnsTheRoot)
{
    const auto tree = run({1, 1}, {{2, 1}, {1, 2}}, {{1, 2}, {2, 1}}, {{0, 0}, {0, 0}});
    ASSERT_EQ(tree.nodes.size(), 1u);
    EXPECT_TRUE(tree.branches.empty());
    EXPECT_EQ(tree.leaves, (vector<size_t>{0}));
    EXPECT_EQ(tree.nodes[0].matching.get_agent_matchs(),
              GetParam() ? (vector<int>{1, 0}) : (vector<int>{0, 1}));
    EXPECT_TRUE(tree.nodes[0].original_ur_stable);
}

TEST_P(RecursiveDA, OverlappingConflictsKeepDuplicateBranchesAndRestoreSiblings)
{
    // Both pairs allow deleting worker 0. Preserve both histories, including
    // their identical child matchings; also exercise a one-direction rejection.
    const Rankings colleagues = {{0, -INF, -INF}, {0, 0, 0}, {0, 0, 0}};
    const Rankings firms = {{3, 2, 1}};
    const auto tree = run({3}, {{1}, {1}, {1}}, firms, colleagues);
    vector<DASearchBranch> root_branches;
    for (const auto &branch : tree.branches) if (branch.from == 0) root_branches.push_back(branch);
    ASSERT_EQ(root_branches.size(), 4u); // Exactly 2 conflicts * 2 endpoints.
    EXPECT_EQ(tree.nodes.size(), 9u);
    EXPECT_EQ(tree.leaves.size(), 6u);
    ASSERT_TRUE(root_branches[0].to);
    ASSERT_TRUE(root_branches[2].to);
    EXPECT_NE(root_branches[0].to, root_branches[2].to);
    EXPECT_EQ(tree.nodes[*root_branches[0].to].matching,
              tree.nodes[*root_branches[2].to].matching);
    EXPECT_NE(root_branches[0].conflict_index, root_branches[2].conflict_index);
    for (const auto &branch : tree.branches) {
        ASSERT_TRUE(branch.to);
        const auto &parent = tree.nodes[branch.from];
        const auto &child = tree.nodes[*branch.to];
        EXPECT_EQ(child.deletions.size(), parent.deletions.size() + 1);
        for (size_t i = 0; i < parent.deletions.size(); ++i) {
            EXPECT_EQ(child.deletions[i].firm, parent.deletions[i].firm);
            EXPECT_EQ(child.deletions[i].worker, parent.deletions[i].worker);
        }
        EXPECT_EQ(child.deletions.back().worker, branch.removed_worker);
        for (const auto &deletion : child.deletions) {
            EXPECT_NE(child.matching.get_agent_matchs()[deletion.worker], deletion.firm);
        }
    }
    EXPECT_EQ(firms, (Rankings{{3, 2, 1}}));
}

TEST_P(RecursiveDA, ScreenshotMatchesEveryExistingReplayPathIncludingDeletionOrder)
{
    const auto problem = screenshot_problem_5x8();
    const auto tree = run(problem.capacities, problem.agent_prefs,
                          problem.firm_prefs, problem.colleague_prefs);
    size_t leaf_index = 0, stable_paths = 0;
    enumerate_choice_paths(
        [&](const DARandomChoiceSelector &selector) {
            const auto algorithm = GetParam() ? iterated_firm_proposing_DA_algorithm
                                              : iterated_doctor_proposing_DA_algorithm;
            return algorithm(8, 5, problem.capacities, problem.agent_prefs,
                             problem.firm_prefs, problem.colleague_prefs, 0, selector);
        },
        [&](uint64_t, Matching &matching, const vector<ChoiceDecision> &decisions) {
            ASSERT_LT(leaf_index, tree.leaves.size());
            const auto &leaf = tree.nodes[tree.leaves[leaf_index++]];
            EXPECT_EQ(leaf.matching, matching);
            ASSERT_EQ(leaf.deletions.size(), decisions.size()); // One conflict at each 5x8 node.
            for (size_t i = 0; i < decisions.size(); ++i) {
                const auto &decision = decisions[i];
                const auto conflict = decision.choice.conflicts.front();
                EXPECT_EQ(leaf.deletions[i].firm, conflict.firm);
                EXPECT_EQ(leaf.deletions[i].worker,
                          decision.selected == 0 ? conflict.left : conflict.right);
            }
            EXPECT_TRUE(leaf.reduced_ur_stable);
            stable_paths += leaf.original_ur_stable;
            for (const auto &blocker : leaf.original_blocking_pairs) {
                EXPECT_TRUE(any_of(leaf.deletions.begin(), leaf.deletions.end(),
                    [&](const DADeletion &d) { return d.worker == blocker.first && d.firm == blocker.second; }));
            }
        });
    EXPECT_EQ(leaf_index, tree.leaves.size());
    EXPECT_EQ(leaf_index, GetParam() ? 3u : 4u);
    EXPECT_EQ(stable_paths, GetParam() ? 3u : 0u);
    for (size_t id = 0; id < tree.nodes.size(); ++id) {
        EXPECT_EQ(count_if(tree.branches.begin(), tree.branches.end(),
                           [&](const DASearchBranch &b) { return b.from == id; }),
                  2 * tree.nodes[id].conflicts.size());
    }
}

TEST_P(RecursiveDA, TracePreservesSeededExecutionAndKeepsAlternativesUnexecuted)
{
    const auto problem = screenshot_problem_5x8();
    const auto trace = GetParam() ? trace_iterated_firm_proposing_DA_algorithm
                                  : trace_iterated_doctor_proposing_DA_algorithm;
    const auto ordinary = GetParam() ? iterated_firm_proposing_DA_algorithm
                                     : iterated_doctor_proposing_DA_algorithm;
    for (unsigned seed = 0; seed < 16; ++seed) {
        const auto tree = trace(8, 5, problem.capacities, problem.agent_prefs,
                                problem.firm_prefs, problem.colleague_prefs, seed, {});
        ASSERT_EQ(tree.leaves.size(), 1u);
        EXPECT_EQ(tree.nodes[tree.leaves[0]].matching,
                  ordinary(8, 5, problem.capacities, problem.agent_prefs,
                           problem.firm_prefs, problem.colleague_prefs, seed, {}));
        for (size_t id = 0; id < tree.nodes.size(); ++id) {
            const auto executed = count_if(tree.branches.begin(), tree.branches.end(),
                [&](const DASearchBranch &b) { return b.from == id && b.to.has_value(); });
            EXPECT_EQ(executed, tree.nodes[id].conflicts.empty() ? 0 : 1);
        }
    }
}

TEST_P(RecursiveDA, TraceKeepsAllPairsEvenWhenTwoBranchesDeleteTheSameWorker)
{
    const auto trace = GetParam() ? trace_iterated_firm_proposing_DA_algorithm
                                  : trace_iterated_doctor_proposing_DA_algorithm;
    const auto tree = trace(3, 1, {3}, {{1}, {1}, {1}}, {{3, 2, 1}},
        {{0, -INF, -INF}, {0, 0, 0}, {0, 0, 0}}, 42,
        [](const DARandomChoice &choice) -> size_t {
            return choice.kind == DAChoiceKind::pair_order ? 1 : 0;
        });
    ASSERT_EQ(tree.branches.size(), 4u);
    EXPECT_FALSE(tree.branches[0].to);
    EXPECT_FALSE(tree.branches[1].to);
    EXPECT_TRUE(tree.branches[2].to);
    EXPECT_FALSE(tree.branches[3].to);
    EXPECT_EQ(tree.leaves.size(), 1u);
}

TEST_P(RecursiveDA, EmptyMarketsAndSystemDispatch)
{
    EXPECT_EQ(run({1}, {}, {{}}, {}).leaves, (vector<size_t>{0}));
    EXPECT_EQ(run({}, {{}}, {}, {{0}}).leaves, (vector<size_t>{0}));
    MatchingSystem system(2, 1);
    system.set_prefs({{1}, {1}}, {{2, 1}}, {{0, -INF}, {0, 0}});
    system.set_firm_capacities({2});
    const string name = GetParam() ? "recursive_firm_propose_DA" : "recursive_doctor_propose_DA";
    const auto tree = system.run_recursive_algorithm(name);
    EXPECT_EQ(tree.leaves.size(), 2u);
    EXPECT_EQ(system.get_firm_prefs(), (Rankings{{2, 1}}));
    EXPECT_THROW(system.run_recursive_algorithm("unknown"), invalid_argument);
    EXPECT_FALSE(tree.nodes[tree.leaves[0]].matching.get_agent_scores().empty());
}

INSTANTIATE_TEST_SUITE_P(ProposingSides, RecursiveDA, testing::Bool(),
    [](const testing::TestParamInfo<bool> &info) { return info.param ? "Firm" : "Doctor"; });

TEST(RecursiveDAExample, ReproducesTheUsersFirstBranchAndContinuesAllOtherBranches)
{
    const auto agents = ranking_scores({{2, 1}, {1}, {1}, {1, 2}}, 2);
    const auto firms = ranking_scores({{1, 2, 3, 4}, {4, 1}}, 4);
    Rankings colleagues(4, vector<int>(4, 0));
    colleagues[0][1] = colleagues[0][2] = -INF;
    const auto tree = recursive_firm_proposing_DA_algorithm(4, 2, {2, 1}, agents, firms, colleagues);
    EXPECT_EQ(tree.nodes[0].matching.get_agent_matchs(), (vector<int>{0, 0, -1, 1}));
    ASSERT_TRUE(tree.branches[0].to);
    EXPECT_EQ(tree.branches[0].removed_worker, 0);
    EXPECT_EQ(tree.nodes[*tree.branches[0].to].matching.get_agent_matchs(), (vector<int>{-1, 0, 0, 1}));
    EXPECT_EQ(tree.leaves.size(), 3u);
}
