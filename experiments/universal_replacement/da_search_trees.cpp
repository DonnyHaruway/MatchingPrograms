#include "problem_5x8.hpp"
#include "algorithms.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

using namespace universal_replacement_experiment;
namespace fs = std::filesystem;

namespace {
Problem problem_without_f5_w8()
{
    auto problem = screenshot_problem_5x8();
    problem.capacities.pop_back();
    problem.firm_rankings.pop_back();
    problem.worker_rankings.pop_back();
    for (auto &ranking : problem.firm_rankings) {
        ranking.erase(std::remove(ranking.begin(), ranking.end(), 8), ranking.end());
    }
    for (auto &ranking : problem.worker_rankings) {
        ranking.erase(std::remove(ranking.begin(), ranking.end(), 5), ranking.end());
    }
    problem.forbidden_pairs.erase(std::remove_if(
        problem.forbidden_pairs.begin(), problem.forbidden_pairs.end(),
        [](const auto &pair) { return pair.first == 8 || pair.second == 8; }),
        problem.forbidden_pairs.end());
    // Restrict the original score matrices without changing any remaining score.
    problem.firm_prefs.pop_back();
    problem.agent_prefs.pop_back();
    problem.colleague_prefs.pop_back();
    for (auto &row : problem.firm_prefs) row.pop_back();
    for (auto &row : problem.agent_prefs) row.pop_back();
    for (auto &row : problem.colleague_prefs) row.pop_back();
    return problem;
}

Problem problem_without_f4_f5_w8()
{
    auto problem = problem_without_f5_w8();
    problem.capacities.pop_back();
    problem.firm_rankings.pop_back();
    problem.firm_prefs.pop_back();
    for (auto &ranking : problem.worker_rankings) {
        ranking.erase(std::remove(ranking.begin(), ranking.end(), 4), ranking.end());
    }
    for (auto &row : problem.agent_prefs) row.pop_back();
    return problem;
}

Problem problem_3x6_without_worker(int removed_worker)
{
    if (removed_worker < 1 || removed_worker > 7) {
        throw std::invalid_argument("worker to remove must be in 1..7");
    }
    auto problem = problem_without_f4_f5_w8();
    const int index = removed_worker - 1;
    problem.worker_rankings.erase(problem.worker_rankings.begin() + index);
    problem.agent_prefs.erase(problem.agent_prefs.begin() + index);
    problem.colleague_prefs.erase(problem.colleague_prefs.begin() + index);
    for (auto &row : problem.firm_prefs) row.erase(row.begin() + index);
    for (auto &row : problem.colleague_prefs) row.erase(row.begin() + index);
    // Algorithms use dense indices. The reports restore the original worker labels.
    for (auto &ranking : problem.firm_rankings) {
        ranking.erase(std::remove(ranking.begin(), ranking.end(), removed_worker), ranking.end());
        for (auto &worker : ranking) if (worker > removed_worker) --worker;
    }
    problem.forbidden_pairs.erase(std::remove_if(
        problem.forbidden_pairs.begin(), problem.forbidden_pairs.end(),
        [removed_worker](const auto &pair) {
            return pair.first == removed_worker || pair.second == removed_worker;
        }),
        problem.forbidden_pairs.end());
    for (auto &[left, right] : problem.forbidden_pairs) {
        if (left > removed_worker) --left;
        if (right > removed_worker) --right;
    }
    return problem;
}

Problem example_problem()
{
    Problem problem;
    problem.capacities = {2, 1};
    problem.firm_rankings = {{1, 2, 3, 4}, {4, 1}};
    problem.worker_rankings = {{2, 1}, {1}, {1}, {1, 2}};
    problem.forbidden_pairs = {{1, 2}, {1, 3}};
    problem.agent_prefs = ranking_scores(problem.worker_rankings, 2);
    problem.firm_prefs = ranking_scores(problem.firm_rankings, 4);
    problem.colleague_prefs.assign(4, std::vector<int>(4, 0));
    for (const auto &[left, right] : problem.forbidden_pairs) {
        problem.colleague_prefs[left - 1][right - 1] = -MatchingTypes::INF;
        problem.colleague_prefs[right - 1][left - 1] = -MatchingTypes::INF;
    }
    return problem;
}

Problem difficult_example_1_problem()
{
    // Input transcribed from 難しい例1.png (3 firms, 5 workers).
    Problem problem;
    problem.capacities = {2, 2, 1};
    problem.firm_rankings = {
        {1, 5, 2, 4, 3},
        {3, 4, 2, 1, 5},
        {1, 4, 5, 3, 2}
    };
    problem.worker_rankings = {
        {3, 1, 2},
        {2, 1, 3},
        {2, 3, 1},
        {3, 1, 2},
        {2, 1, 3}
    };
    problem.forbidden_pairs = {{2, 3}, {2, 5}};
    problem.agent_prefs = ranking_scores(problem.worker_rankings, 3);
    problem.firm_prefs = ranking_scores(problem.firm_rankings, 5);
    problem.colleague_prefs.assign(5, std::vector<int>(5, 0));
    for (const auto &[left, right] : problem.forbidden_pairs) {
        problem.colleague_prefs[left - 1][right - 1] = -MatchingTypes::INF;
        problem.colleague_prefs[right - 1][left - 1] = -MatchingTypes::INF;
    }
    return problem;
}

void vector_json(std::ostream &out, const std::vector<int> &values, int offset = 0)
{
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << values[i] + offset;
    }
    out << ']';
}

void matrix_json(std::ostream &out, const Rankings &matrix)
{
    out << '[';
    for (std::size_t i = 0; i < matrix.size(); ++i) {
        if (i) out << ',';
        vector_json(out, matrix[i]);
    }
    out << ']';
}

void write_problem(const fs::path &path, const Problem &problem,
                   const std::vector<int> &worker_labels)
{
    std::ofstream out(path);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << "{\"n_firms\":" << problem.firm_prefs.size()
        << ",\"n_workers\":" << problem.agent_prefs.size() << ",\"capacities\":";
    vector_json(out, problem.capacities);
    out << ",\"agent_prefs\":"; matrix_json(out, problem.agent_prefs);
    out << ",\"firm_prefs\":"; matrix_json(out, problem.firm_prefs);
    out << ",\"colleague_prefs\":"; matrix_json(out, problem.colleague_prefs);
    out << ",\"worker_rankings_one_based\":"; matrix_json(out, problem.worker_rankings);
    out << ",\"firm_rankings_one_based\":"; matrix_json(out, problem.firm_rankings);
    if (!worker_labels.empty()) {
        out << ",\"worker_labels_one_based\":";
        vector_json(out, worker_labels);
    }
    out << "}\n";
}

void write_tree(const fs::path &directory, const std::string &name, const DASearchTree &tree,
                unsigned seed)
{
    std::ofstream out(directory / (name + ".json"));
    out.exceptions(std::ios::failbit | std::ios::badbit);
    const bool recursive = name.find("recursive_") == 0;
    const bool firm = name.find("firm") != std::string::npos;
    out << "{\"schema_version\":1,\"algorithm\":\"" << name
        << "\",\"mode\":\"" << (recursive ? "recursive" : "iterated")
        << "\",\"proposing_side\":\"" << (firm ? "firm" : "doctor") << "\",\"seed\":";
    if (recursive) out << "null"; else out << seed;
    out << ",\"nodes\":[";
    for (std::size_t id = 0; id < tree.nodes.size(); ++id) {
        if (id) out << ',';
        const auto &node = tree.nodes[id];
        out << "{\"id\":" << id << ",\"da_run\":" << node.deletions.size() + 1
            << ",\"assignment\":";
        vector_json(out, node.matching.get_agent_matchs(), 1); // 0 = unmatched; others one-based.
        out << ",\"conflicts\":[";
        for (std::size_t i = 0; i < node.conflicts.size(); ++i) {
            if (i) out << ',';
            const auto &pair = node.conflicts[i];
            out << "{\"firm\":" << pair.firm + 1 << ",\"left\":" << pair.left + 1
                << ",\"right\":" << pair.right + 1 << '}';
        }
        out << "],\"deletions\":[";
        for (std::size_t i = 0; i < node.deletions.size(); ++i) {
            if (i) out << ',';
            const auto &d = node.deletions[i];
            out << "{\"firm\":" << d.firm + 1 << ",\"worker\":" << d.worker + 1 << '}';
        }
        out << "],\"original_ur_stable\":" << (node.original_ur_stable ? "true" : "false")
            << ",\"reduced_ur_stable\":" << (node.reduced_ur_stable ? "true" : "false")
            << ",\"original_blocking_pairs\":[";
        for (std::size_t i = 0; i < node.original_blocking_pairs.size(); ++i) {
            if (i) out << ',';
            const auto &[worker, employer] = node.original_blocking_pairs[i];
            out << "{\"firm\":" << employer + 1 << ",\"worker\":" << worker + 1 << '}';
        }
        out << "]}";
    }
    out << "],\"branches\":[";
    for (std::size_t id = 0; id < tree.branches.size(); ++id) {
        if (id) out << ',';
        const auto &branch = tree.branches[id];
        const auto &parent = tree.nodes[branch.from];
        const auto &pair = parent.conflicts[branch.conflict_index];
        const int retained = branch.removed_worker == pair.left ? pair.right : pair.left;
        out << "{\"id\":" << id << ",\"from\":" << branch.from << ",\"to\":";
        if (branch.to) out << *branch.to; else out << "null";
        out << ",\"conflict_index\":" << branch.conflict_index << ",\"firm\":" << pair.firm + 1
            << ",\"removed_worker\":" << branch.removed_worker + 1
            << ",\"retained_worker\":" << retained + 1
            << ",\"removed_before\":" << parent.matching.get_agent_matchs()[branch.removed_worker] + 1
            << ",\"retained_before\":" << parent.matching.get_agent_matchs()[retained] + 1;
        if (branch.to) {
            const auto &after = tree.nodes[*branch.to].matching.get_agent_matchs();
            out << ",\"removed_after\":" << after[branch.removed_worker] + 1
                << ",\"retained_after\":" << after[retained] + 1
                << ",\"retained_moved\":" << (after[retained] != pair.firm ? "true" : "false");
        } else {
            out << ",\"removed_after\":null,\"retained_after\":null,\"retained_moved\":null";
        }
        out << '}';
    }
    out << "],\"leaves\":[";
    std::size_t stable = 0;
    for (std::size_t i = 0; i < tree.leaves.size(); ++i) {
        if (i) out << ',';
        out << tree.leaves[i];
        stable += tree.nodes[tree.leaves[i]].original_ur_stable;
    }
    out << "]}\n";
    std::cout << name << ": nodes=" << tree.nodes.size() << ", leaves=" << tree.leaves.size()
              << ", original UR stable leaves=" << stable << '\n';
}
} // namespace

int main(int argc, char **argv)
{
    try {
        std::string profile = "5x8";
        unsigned seed = 42;
        fs::path output;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "Usage: da_search_trees [--profile PROFILE] [--seed N] [--output NEW_DIRECTORY]\n"
                          << "Profiles: 5x8, 4x7 (without f5,w8), 3x7 (without f4,f5,w8), "
                          << "3x6 (without f4,f5,w5,w8), 3x6-without-w4, 3x6-without-w6, example, difficult-example-1\n";
                return 0;
            }
            if (i + 1 == argc) throw std::invalid_argument("missing option value: " + option);
            const std::string value = argv[++i];
            if (option == "--profile") profile = value;
            else if (option == "--output") output = value;
            else if (option == "--seed") {
                std::size_t used = 0;
                const auto number = std::stoull(value, &used);
                if (used != value.size() || value.front() == '-'
                    || number > std::numeric_limits<unsigned>::max()) {
                    throw std::invalid_argument("invalid unsigned seed");
                }
                seed = static_cast<unsigned>(number);
            } else throw std::invalid_argument("unknown option: " + option);
        }
        if (profile != "5x8" && profile != "4x7" && profile != "3x7"
            && profile != "3x6" && profile != "3x6-without-w4"
            && profile != "3x6-without-w6" && profile != "example"
            && profile != "difficult-example-1") {
            throw std::invalid_argument("unknown profile");
        }
        const int removed_worker = profile == "3x6" ? 5
                                 : profile == "3x6-without-w4" ? 4
                                 : profile == "3x6-without-w6" ? 6 : 0;
        const auto problem = profile == "5x8" ? screenshot_problem_5x8()
                           : profile == "4x7" ? problem_without_f5_w8()
                           : profile == "3x7" ? problem_without_f4_f5_w8()
                           : profile == "difficult-example-1" ? difficult_example_1_problem()
                           : removed_worker ? problem_3x6_without_worker(removed_worker) : example_problem();
        if (output.empty()) {
            const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            output = fs::path("experiments/universal_replacement/outputs") / ("da_trees_" + std::to_string(stamp));
        }
        if (!output.parent_path().empty()) fs::create_directories(output.parent_path());
        if (!fs::create_directory(output)) throw std::runtime_error("output directory already exists");
        std::vector<int> worker_labels;
        if (removed_worker) {
            for (int worker = 1; worker <= 7; ++worker) {
                if (worker != removed_worker) worker_labels.push_back(worker);
            }
        }
        write_problem(output / "problem.json", problem, worker_labels);
        const int workers = problem.agent_prefs.size(), firms = problem.firm_prefs.size();
        write_tree(output, "iterated_firm", trace_iterated_firm_proposing_DA_algorithm(
            workers, firms, problem.capacities, problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, seed), seed);
        write_tree(output, "iterated_doctor", trace_iterated_doctor_proposing_DA_algorithm(
            workers, firms, problem.capacities, problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, seed), seed);
        write_tree(output, "recursive_firm", recursive_firm_proposing_DA_algorithm(
            workers, firms, problem.capacities, problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs), seed);
        write_tree(output, "recursive_doctor", recursive_doctor_proposing_DA_algorithm(
            workers, firms, problem.capacities, problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs), seed);
        std::cout << "Outputs: " << fs::absolute(output) << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
