#pragma once

#include "algorithms.hpp"
#include "problem_5x8.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>

namespace rda_batch {

using universal_replacement_experiment::Problem;
using universal_replacement_experiment::Rankings;

inline std::uint64_t mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

inline Problem make_problem(std::uint64_t seed, std::uint64_t trial, bool incomplete)
{
    Problem problem;
    problem.capacities = {2, 2, 2, 2, 1};
    const auto profile_seed = mix(seed ^ mix(trial));
    std::mt19937_64 rank_rng(mix(profile_seed ^ 0x52414e4bULL));
    std::mt19937_64 colleague_rng(mix(profile_seed ^ 0x434f4c4cULL));
    const std::vector<int> worker_lengths = {4, 3, 4, 4, 2, 4, 4, 2};
    const std::vector<int> firm_lengths = {7, 6, 7, 3, 4};
    for (int worker = 0; worker < 8; ++worker) {
        std::vector<int> order(5);
        std::iota(order.begin(), order.end(), 1);
        std::shuffle(order.begin(), order.end(), rank_rng);
        if (incomplete) order.resize(worker_lengths[worker]);
        problem.worker_rankings.push_back(std::move(order));
    }
    for (int firm = 0; firm < 5; ++firm) {
        std::vector<int> order(8);
        std::iota(order.begin(), order.end(), 1);
        std::shuffle(order.begin(), order.end(), rank_rng);
        if (incomplete) order.resize(firm_lengths[firm]);
        problem.firm_rankings.push_back(std::move(order));
    }
    std::vector<std::pair<int, int>> all_pairs;
    for (int left = 1; left <= 8; ++left) {
        for (int right = left + 1; right <= 8; ++right) all_pairs.emplace_back(left, right);
    }
    std::shuffle(all_pairs.begin(), all_pairs.end(), colleague_rng);
    problem.forbidden_pairs.assign(all_pairs.begin(), all_pairs.begin() + 9);
    std::sort(problem.forbidden_pairs.begin(), problem.forbidden_pairs.end());
    problem.colleague_prefs.assign(8, std::vector<int>(8, 0));
    for (const auto &[left, right] : problem.forbidden_pairs) {
        problem.colleague_prefs[left - 1][right - 1] = -MatchingTypes::INF;
        problem.colleague_prefs[right - 1][left - 1] = -MatchingTypes::INF;
    }
    problem.agent_prefs = universal_replacement_experiment::ranking_scores(problem.worker_rankings, 5);
    problem.firm_prefs = universal_replacement_experiment::ranking_scores(problem.firm_rankings, 8);
    return problem;
}

struct Summary {
    std::uint64_t nodes = 0, branches = 0, leaves = 0, stable_leaves = 0;
    std::size_t distinct_matchings = 0, distinct_stable_matchings = 0;
    std::size_t initial_conflicts = 0, max_conflicts = 0, max_deletions = 0;
    int min_matched_workers = 8, max_matched_workers = 0;
    std::optional<std::size_t> stable_witness, unstable_witness;
    double seconds = 0;

    std::string category() const
    {
        if (leaves == 0) throw std::logic_error("an exhaustive search must have a leaf");
        return stable_leaves == leaves ? "all_stable" : stable_leaves == 0 ? "none_stable" : "mixed";
    }
};

inline Summary summarize(const DASearchTree &tree, const Problem &problem)
{
    if (tree.nodes.empty()) throw std::logic_error("missing root");
    Summary result;
    result.nodes = tree.nodes.size();
    result.branches = tree.branches.size();
    result.leaves = tree.leaves.size();
    result.initial_conflicts = tree.nodes[0].conflicts.size();
    std::set<std::vector<int>> distinct, stable;
    std::vector<std::size_t> degrees(tree.nodes.size(), 0);
    for (const auto &branch : tree.branches) {
        if (!branch.to || *branch.to <= branch.from || *branch.to >= tree.nodes.size()) {
            throw std::logic_error("recursive branch is not executed or not a tree edge");
        }
        ++degrees[branch.from];
    }
    for (std::size_t id = 0; id < tree.nodes.size(); ++id) {
        const auto &node = tree.nodes[id];
        if (degrees[id] != 2 * node.conflicts.size()) throw std::logic_error("incorrect branch count");
        result.max_conflicts = std::max(result.max_conflicts, node.conflicts.size());
        result.max_deletions = std::max(result.max_deletions, node.deletions.size());
    }
    if (result.nodes != result.branches + 1) throw std::logic_error("incorrect tree size");
    for (const auto id : tree.leaves) {
        const auto &leaf = tree.nodes.at(id);
        auto matching = leaf.matching;
        if (!leaf.conflicts.empty() || !leaf.reduced_ur_stable
            || !matching.is_individually_rational(problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs)
            || leaf.original_ur_stable != leaf.original_blocking_pairs.empty()) {
            throw std::logic_error("leaf feasibility / UR consistency failure");
        }
        for (const auto &blocker : leaf.original_blocking_pairs) {
            const bool deleted = std::any_of(leaf.deletions.begin(), leaf.deletions.end(),
                [&](const DADeletion &d) { return d.worker == blocker.first && d.firm == blocker.second; });
            if (!deleted) throw std::logic_error("original blocking pair was not deleted");
        }
        const auto &assignment = matching.get_agent_matchs();
        const int matched = static_cast<int>(std::count_if(assignment.begin(), assignment.end(),
            [](int firm) { return firm != MatchingTypes::UNMATCHED; }));
        result.min_matched_workers = std::min(result.min_matched_workers, matched);
        result.max_matched_workers = std::max(result.max_matched_workers, matched);
        distinct.insert(assignment);
        if (leaf.original_ur_stable) {
            ++result.stable_leaves;
            stable.insert(assignment);
            if (!result.stable_witness) result.stable_witness = id;
        } else if (!result.unstable_witness) result.unstable_witness = id;
    }
    result.distinct_matchings = distinct.size();
    result.distinct_stable_matchings = stable.size();
    result.category();
    return result;
}

inline std::vector<std::size_t> history(const DASearchTree &tree, std::size_t leaf)
{
    std::map<std::size_t, std::size_t> incoming;
    for (std::size_t id = 0; id < tree.branches.size(); ++id) {
        const auto &branch = tree.branches[id];
        if (branch.to) incoming.emplace(*branch.to, id);
    }
    std::vector<std::size_t> result;
    while (leaf != 0) {
        const auto branch = incoming.at(leaf);
        result.push_back(branch);
        leaf = tree.branches[branch].from;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace rda_batch
