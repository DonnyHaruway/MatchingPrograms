#include <gtest/gtest.h>
#include "../../experiments/universal_replacement/rda_exhaustive.hpp"

using namespace rda_exhaustive;

namespace {
Count encode(const Domain &domain, const Problem &problem)
{
    Count id = 0;
    const auto append = [&](const Rankings &orders, const std::vector<StrictOrder> &options) {
        for (const auto &order : orders) {
            const auto it = std::find_if(options.begin(), options.end(),
                [&](const StrictOrder &candidate) { return candidate.order == order; });
            if (it == options.end()) throw std::logic_error("order missing from domain");
            id = id * options.size() + static_cast<Count>(it - options.begin());
        }
    };
    append(problem.worker_rankings, domain.worker_orders);
    append(problem.firm_rankings, domain.firm_orders);
    Count mask = 0;
    unsigned bit = 0;
    for (int left = 0; left < domain.workers; ++left) {
        for (int right = left + 1; right < domain.workers; ++right, ++bit) {
            if (problem.colleague_prefs[left][right] == -MatchingTypes::INF) mask |= Count{1} << bit;
        }
    }
    return id * domain.masks + mask;
}

// Independent tiny-market reference: scan firms repeatedly rather than using
// the production queue/sets, then test every possible single-worker replacement.
std::vector<int> reference_da(const Problem &problem, const Rankings &firms)
{
    const int n = static_cast<int>(problem.agent_prefs.size());
    std::vector<int> assignment(n, -1);
    std::vector<std::vector<bool>> tried(2, std::vector<bool>(n, false));
    while (true) {
        bool proposed = false;
        for (int f = 0; f < 2; ++f) {
            if (std::count(assignment.begin(), assignment.end(), f) >= problem.capacities[f]) continue;
            int best = -1;
            for (int w = 0; w < n; ++w) {
                if (!tried[f][w] && firms[f][w] > 0
                    && (best == -1 || firms[f][w] > firms[f][best])) best = w;
            }
            if (best == -1) continue;
            proposed = true;
            tried[f][best] = true;
            const int old = assignment[best];
            const int old_score = old == -1 ? 0 : problem.agent_prefs[best][old];
            if (problem.agent_prefs[best][f] > old_score) assignment[best] = f;
        }
        if (!proposed) return assignment;
    }
}

bool compatible(const Problem &problem, const std::vector<int> &workers)
{
    for (int left : workers) {
        for (int right : workers) {
            if (left != right && problem.colleague_prefs[left][right] == -MatchingTypes::INF) return false;
        }
    }
    return true;
}

bool reference_stable(const Problem &problem, const std::vector<int> &assignment)
{
    const int n = static_cast<int>(assignment.size());
    std::vector<std::vector<int>> incumbents(2);
    for (int w = 0; w < n; ++w) {
        const int f = assignment[w];
        if (f == -1) continue;
        if (problem.agent_prefs[w][f] <= 0 || problem.firm_prefs[f][w] <= 0) return false;
        incumbents[f].push_back(w);
    }
    for (int f = 0; f < 2; ++f) {
        if (incumbents[f].size() > static_cast<std::size_t>(problem.capacities[f])
            || !compatible(problem, incumbents[f])) return false;
        for (int w = 0; w < n; ++w) {
            const int old = assignment[w];
            if (old == f || problem.agent_prefs[w][f] <= (old == -1 ? 0 : problem.agent_prefs[w][old])) continue;
            auto after = incumbents[f];
            if (after.size() < static_cast<std::size_t>(problem.capacities[f])) {
                after.push_back(w);
                if (problem.firm_prefs[f][w] > 0 && compatible(problem, after)) return false;
            } else {
                bool improves = false, every_replacement_works = true;
                for (std::size_t i = 0; i < incumbents[f].size(); ++i) {
                    improves |= problem.firm_prefs[f][w] > problem.firm_prefs[f][incumbents[f][i]];
                    after = incumbents[f];
                    after[i] = w;
                    every_replacement_works &= compatible(problem, after);
                }
                if (improves && every_replacement_works) return false;
            }
        }
    }
    return true;
}

struct Reference {
    Count nodes = 0, max_depth = 0;
    std::multiset<std::vector<int>> leaves;
};

void reference_visit(const Problem &problem, Rankings &current, Count depth, Reference &result)
{
    ++result.nodes;
    result.max_depth = std::max(result.max_depth, depth);
    auto assignment = reference_da(problem, current);
    bool conflict = false;
    for (int left = 0; left < static_cast<int>(assignment.size()); ++left) {
        for (int right = left + 1; right < static_cast<int>(assignment.size()); ++right) {
            if (assignment[left] == -1 || assignment[left] != assignment[right]
                || problem.colleague_prefs[left][right] != -MatchingTypes::INF) continue;
            conflict = true;
            const int firm = assignment[left];
            for (int removed : {left, right}) {
                const int old = current[firm][removed];
                current[firm][removed] = -MatchingTypes::INF;
                reference_visit(problem, current, depth + 1, result);
                current[firm][removed] = old;
            }
        }
    }
    if (!conflict) {
        assignment.push_back(reference_stable(problem, assignment) ? 1 : 0);
        result.leaves.insert(assignment);
    }
}
} // namespace

TEST(RDAExhaustive, ExactCountsCapacitiesAndRangeValidation)
{
    const Domain three(3), four(4);
    EXPECT_EQ(three.total, 995328u);
    EXPECT_EQ(four.total, 1194393600u);
    EXPECT_EQ(three.capacities, (std::vector<int>{2, 1}));
    EXPECT_EQ(four.capacities, (std::vector<int>{2, 2}));
    EXPECT_EQ(three.masks, 8u);
    EXPECT_EQ(four.masks, 64u);
    EXPECT_EQ(range(three, 1, std::nullopt).size(), three.total - 1);
    EXPECT_EQ(range(three, three.total - 1, 1).end, three.total);
    EXPECT_THROW(Domain(2), std::invalid_argument);
    EXPECT_THROW(range(three, 0, 0), std::invalid_argument);
    EXPECT_THROW(range(three, three.total, 1), std::invalid_argument);
    EXPECT_THROW(range(three, three.total - 1, 2), std::invalid_argument);
    EXPECT_THROW(range(three, 1, std::numeric_limits<Count>::max()), std::invalid_argument);
    EXPECT_THROW(three.problem(three.total), std::out_of_range);
}

TEST(RDAExhaustive, AllStrictOrdersIncludeEveryOutsidePositionAndRetainUnacceptableRanks)
{
    for (int n : {2, 3, 4}) {
        const auto options = strict_orders(n);
        std::set<std::vector<int>> orders, scores;
        std::vector<int> positions(n + 1, 0);
        for (const auto &option : options) {
            orders.insert(option.order);
            scores.insert(option.scores);
            const int outside = static_cast<int>(std::find(option.order.begin(), option.order.end(), 0) - option.order.begin());
            ++positions[outside];
            for (int rank = 0; rank <= n; ++rank) {
                if (option.order[rank] != 0) EXPECT_EQ(option.scores[option.order[rank] - 1], outside - rank);
            }
        }
        EXPECT_EQ(orders.size(), options.size());
        EXPECT_EQ(scores.size(), options.size());
        for (int count : positions) EXPECT_EQ(count, static_cast<int>(options.size()) / (n + 1));
    }
    const auto options = strict_orders(2);
    EXPECT_EQ(options[0].order, (std::vector<int>{0, 1, 2}));
    EXPECT_EQ(options[1].order, (std::vector<int>{0, 2, 1}));
    EXPECT_NE(options[0].scores, options[1].scores);
}

TEST(RDAExhaustive, InputIDsCoverEvery2x3ProfileAndForbiddenMaskWithoutCollisions)
{
    const Domain domain(3);
    for (Count id = 0; id < domain.total; ++id) {
        const auto problem = domain.problem(id);
        ASSERT_EQ(encode(domain, problem), id);
        ASSERT_EQ(problem.forbidden_pairs.size(), static_cast<std::size_t>(__builtin_popcountll(id % domain.masks)));
        for (int left = 0; left < 3; ++left) {
            ASSERT_EQ(problem.colleague_prefs[left][left], 0);
            for (int right = 0; right < 3; ++right) {
                ASSERT_EQ(problem.colleague_prefs[left][right], problem.colleague_prefs[right][left]);
            }
        }
    }
    const Domain four(4);
    for (Count id : {Count{0}, Count{63}, Count{64}, four.total / 2, four.total - 1}) {
        EXPECT_EQ(encode(four, four.problem(id)), id);
    }
}

TEST(RDAExhaustive, AllBranchesAndOriginalURStabilityAgreeWithIndependentReference)
{
    Count total_nontrivial = 0, unstable = 0;
    for (int n : {3, 4}) {
        const Domain domain(n);
        for (Count sample = 0; sample < 1000; ++sample) {
            const Count id = (sample * (domain.total - 1)) / 999;
            SCOPED_TRACE("workers=" + std::to_string(n) + " id=" + std::to_string(id));
            const auto problem = domain.problem(id);
            auto current = problem.firm_prefs;
            Reference reference;
            reference_visit(problem, current, 0, reference);
            const auto tree = recursive_firm_proposing_DA_algorithm(n, 2, domain.capacities,
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs);
            const auto summary = rda_batch::summarize(tree, problem);
            std::multiset<std::vector<int>> leaves;
            for (const auto leaf : tree.leaves) {
                auto assignment = tree.nodes[leaf].matching.get_agent_matchs();
                assignment.push_back(tree.nodes[leaf].original_ur_stable ? 1 : 0);
                leaves.insert(std::move(assignment));
            }
            EXPECT_EQ(summary.nodes, reference.nodes);
            EXPECT_EQ(summary.max_deletions, reference.max_depth);
            EXPECT_EQ(leaves, reference.leaves);
            total_nontrivial += summary.initial_conflicts > 0;
            unstable += summary.leaves - summary.stable_leaves;
        }
    }
    EXPECT_GT(total_nontrivial, 0u);
    EXPECT_GT(unstable, 0u);
}
