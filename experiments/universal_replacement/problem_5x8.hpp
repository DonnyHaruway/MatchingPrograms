#pragma once

#include "CommonTypes.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace universal_replacement_experiment {

using Rankings = std::vector<std::vector<int>>;

struct Problem {
    // Rankings and forbidden pairs below use the image's one-based labels.
    Rankings firm_rankings;
    Rankings worker_rankings;
    std::vector<int> capacities;
    std::vector<std::pair<int, int>> forbidden_pairs;
    Rankings agent_prefs;
    Rankings firm_prefs;
    Rankings colleague_prefs;
};

inline Rankings ranking_scores(const Rankings &rankings, int alternatives)
{
    Rankings scores(rankings.size(), std::vector<int>(alternatives, -MatchingTypes::INF));
    for (std::size_t owner = 0; owner < rankings.size(); ++owner) {
        for (std::size_t rank = 0; rank < rankings[owner].size(); ++rank) {
            const int label = rankings[owner][rank];
            if (label < 1 || label > alternatives
                || scores[owner][label - 1] != -MatchingTypes::INF) {
                throw std::invalid_argument("invalid or duplicate ranking label");
            }
            scores[owner][label - 1] = static_cast<int>(rankings[owner].size() - rank);
        }
    }
    return scores;
}

inline Problem screenshot_problem_5x8()
{
    Problem problem;
    problem.capacities = {2, 2, 2, 2, 1};
    problem.firm_rankings = {
        {2, 6, 7, 1, 5, 8, 3},
        {3, 1, 6, 2, 7, 4},
        {7, 4, 2, 5, 1, 3, 6},
        {6, 7, 4},
        {8, 1, 4, 3}
    };
    problem.worker_rankings = {
        {1, 3, 2, 5},
        {2, 1, 3},
        {3, 2, 5, 1},
        {2, 3, 4, 5},
        {1, 3},
        {3, 2, 1, 4},
        {2, 1, 3, 4},
        {1, 5}
    };
    problem.forbidden_pairs = {
        {2, 3}, {2, 6}, {2, 8}, {3, 5}, {4, 5},
        {5, 7}, {5, 8}, {6, 8}, {7, 8}
    };
    problem.agent_prefs = ranking_scores(problem.worker_rankings, 5);
    problem.firm_prefs = ranking_scores(problem.firm_rankings, 8);
    problem.colleague_prefs.assign(8, std::vector<int>(8, 0));
    for (const auto &[left, right] : problem.forbidden_pairs) {
        if (left < 1 || left > 8 || right < 1 || right > 8 || left == right) {
            throw std::invalid_argument("invalid forbidden pair");
        }
        problem.colleague_prefs[left - 1][right - 1] = -MatchingTypes::INF;
        problem.colleague_prefs[right - 1][left - 1] = -MatchingTypes::INF;
    }
    return problem;
}

} // namespace universal_replacement_experiment
