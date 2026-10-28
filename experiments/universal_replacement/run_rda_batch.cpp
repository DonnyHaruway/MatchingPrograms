#include "rda_batch.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

namespace fs = std::filesystem;
using namespace rda_batch;

namespace {
template<class T>
void array_json(std::ostream &out, const std::vector<T> &values, int offset = 0)
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
        array_json(out, matrix[i]);
    }
    out << ']';
}

void assignment_json(std::ostream &out, const Matching &matching)
{
    array_json(out, matching.get_agent_matchs(), 1); // Public labels: 1..5, 0 = unmatched.
}

void blockers_json(std::ostream &out, const DASearchNode &node)
{
    out << '[';
    for (std::size_t i = 0; i < node.original_blocking_pairs.size(); ++i) {
        if (i) out << ',';
        const auto &[worker, firm] = node.original_blocking_pairs[i];
        out << "{\"firm\":" << firm + 1 << ",\"worker\":" << worker + 1 << '}';
    }
    out << ']';
}

void witness_json(std::ostream &out, const DASearchTree &tree, std::optional<std::size_t> leaf)
{
    if (!leaf) { out << "null"; return; }
    const auto &terminal = tree.nodes[*leaf];
    out << "{\"leaf_id\":" << *leaf << ",\"assignment\":";
    assignment_json(out, terminal.matching);
    out << ",\"original_ur_stable\":" << (terminal.original_ur_stable ? "true" : "false")
        << ",\"reduced_ur_stable\":" << (terminal.reduced_ur_stable ? "true" : "false")
        << ",\"original_blocking_pairs\":";
    blockers_json(out, terminal);
    out << ",\"steps\":[";
    const auto path = history(tree, *leaf);
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i) out << ',';
        const auto &branch = tree.branches[path[i]];
        const auto &before = tree.nodes[branch.from];
        const auto &after = tree.nodes[*branch.to];
        const auto &pair = before.conflicts[branch.conflict_index];
        const int retained = branch.removed_worker == pair.left ? pair.right : pair.left;
        out << "{\"parent_node\":" << branch.from << ",\"child_node\":" << *branch.to
            << ",\"conflict_index\":" << branch.conflict_index
            << ",\"firm\":" << pair.firm + 1 << ",\"left\":" << pair.left + 1
            << ",\"right\":" << pair.right + 1 << ",\"removed_worker\":" << branch.removed_worker + 1
            << ",\"retained_worker\":" << retained + 1 << ",\"assignment_before\":";
        assignment_json(out, before.matching);
        out << ",\"assignment_after\":";
        assignment_json(out, after.matching);
        out << ",\"retained_after\":" << after.matching.get_agent_matchs()[retained] + 1
            << ",\"retained_final\":" << terminal.matching.get_agent_matchs()[retained] + 1 << '}';
    }
    out << "]}";
}

void outcome_json(std::ostream &out, const DASearchTree &tree, const Summary &result)
{
    out << "{\"category\":\"" << result.category() << "\",\"nodes\":" << result.nodes
        << ",\"branches\":" << result.branches << ",\"leaves\":" << result.leaves
        << ",\"stable_leaves\":" << result.stable_leaves
        << ",\"unstable_leaves\":" << result.leaves - result.stable_leaves
        << ",\"distinct_matchings\":" << result.distinct_matchings
        << ",\"distinct_stable_matchings\":" << result.distinct_stable_matchings
        << ",\"initial_conflicts\":" << result.initial_conflicts
        << ",\"max_conflicts\":" << result.max_conflicts
        << ",\"max_deletions\":" << result.max_deletions
        << ",\"min_matched_workers\":" << result.min_matched_workers
        << ",\"max_matched_workers\":" << result.max_matched_workers
        << ",\"algorithm_seconds\":" << result.seconds
        << ",\"stable_witness\":";
    witness_json(out, tree, result.stable_witness);
    out << ",\"unstable_witness\":";
    witness_json(out, tree, result.unstable_witness);
    out << '}';
}

void problem_json(std::ostream &out, const Problem &problem)
{
    out << "{\"n_firms\":5,\"n_workers\":8,\"capacities\":";
    array_json(out, problem.capacities);
    out << ",\"agent_prefs\":"; matrix_json(out, problem.agent_prefs);
    out << ",\"firm_prefs\":"; matrix_json(out, problem.firm_prefs);
    out << ",\"colleague_prefs\":"; matrix_json(out, problem.colleague_prefs);
    out << ",\"worker_rankings_one_based\":"; matrix_json(out, problem.worker_rankings);
    out << ",\"firm_rankings_one_based\":"; matrix_json(out, problem.firm_rankings);
    out << ",\"forbidden_pairs\":[";
    for (std::size_t i = 0; i < problem.forbidden_pairs.size(); ++i) {
        if (i) out << ',';
        out << '[' << problem.forbidden_pairs[i].first << ',' << problem.forbidden_pairs[i].second << ']';
    }
    out << "]}";
}

std::uint64_t unsigned_value(const std::string &value)
{
    std::size_t used = 0;
    const auto number = std::stoull(value, &used);
    if (used != value.size() || value.front() == '-') throw std::invalid_argument("invalid unsigned number");
    return number;
}
} // namespace

int main(int argc, char **argv)
{
    try {
        std::uint64_t trials = 1000, seed = 20261021, start_trial = 0;
        fs::path output;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "Usage: run_rda_batch --output NEW_DIRECTORY [--trials 1000] [--seed 20261021] [--start-trial 0]\n";
                return 0;
            }
            if (i + 1 == argc) throw std::invalid_argument("missing option value: " + option);
            const std::string value = argv[++i];
            if (option == "--output") output = value;
            else if (option == "--trials") trials = unsigned_value(value);
            else if (option == "--seed") seed = unsigned_value(value);
            else if (option == "--start-trial") start_trial = unsigned_value(value);
            else throw std::invalid_argument("unknown option: " + option);
        }
        if (trials == 0 || start_trial > std::numeric_limits<std::uint64_t>::max() - trials) {
            throw std::invalid_argument("invalid trial range");
        }
        if (output.empty()) throw std::invalid_argument("--output is required");
        if (!output.parent_path().empty()) fs::create_directories(output.parent_path());
        if (!fs::create_directory(output)) throw std::runtime_error("output directory already exists");
        {
            std::ofstream metadata(output / "config.json");
            metadata.exceptions(std::ios::failbit | std::ios::badbit);
            metadata << "{\"schema_version\":1,\"seed\":" << seed << ",\"trials_per_domain\":" << trials
                     << ",\"start_trial\":" << start_trial
                     << ",\"domains\":[\"complete\",\"incomplete\"],\"n_firms\":5,\"n_workers\":8,"
                     << "\"capacities\":[2,2,2,2,1],\"forbidden_pairs\":9,\"colleague_mode\":\"symmetric\","
                     << "\"worker_list_lengths\":[4,3,4,4,2,4,4,2],\"firm_list_lengths\":[7,6,7,3,4],"
                     << "\"paired_domains\":true,\"vacancy_blocking\":true,\"full_path_enumeration\":true,"
                     << "\"duplicate_branches_kept\":true,\"early_stopping\":false,\"figures_generated\":false}\n";
        }
        std::ofstream data(output / "instances.jsonl");
        data.exceptions(std::ios::failbit | std::ios::badbit);
        data << std::setprecision(12);
        const auto started = std::chrono::steady_clock::now();
        std::uint64_t total_nodes = 0, largest_tree = 0;
        std::map<std::string, std::map<std::string, std::uint64_t>> categories;
        for (std::uint64_t index = 0; index < trials; ++index) {
            const auto trial = start_trial + index;
            for (bool incomplete : {false, true}) {
                const std::string domain = incomplete ? "incomplete" : "complete";
                const auto problem = make_problem(seed, trial, incomplete);
                data << "{\"domain\":\"" << domain << "\",\"trial\":" << trial << ",\"seed\":" << seed << ",\"problem\":";
                problem_json(data, problem);
                data << ",\"algorithms\":{";
                for (bool firm : {true, false}) {
                    const std::string side = firm ? "firm" : "doctor";
                    const auto before = std::chrono::steady_clock::now();
                    const auto run = firm ? recursive_firm_proposing_DA_algorithm : recursive_doctor_proposing_DA_algorithm;
                    const auto tree = run(8, 5, problem.capacities, problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs);
                    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count();
                    auto result = summarize(tree, problem);
                    result.seconds = seconds;
                    if (!firm) data << ',';
                    data << '"' << side << "\":";
                    outcome_json(data, tree, result);
                    total_nodes += result.nodes;
                    largest_tree = std::max(largest_tree, result.nodes);
                    ++categories[domain + "/" + side][result.category()];
                }
                data << "}}\n";
                data.flush();
            }
            if ((index + 1) % 25 == 0 || index + 1 == trials) {
                const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
                std::cout << "Profiles per domain: " << index + 1 << '/' << trials << "; nodes=" << total_nodes
                          << "; largest_tree=" << largest_tree << "; elapsed_seconds=" << seconds << std::endl;
            }
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::ofstream completion(output / "completion.json");
        completion.exceptions(std::ios::failbit | std::ios::badbit);
        completion << std::setprecision(12) << "{\"complete\":true,\"profiles_per_domain\":" << trials
                   << ",\"records\":" << 2 * trials << ",\"total_nodes\":" << total_nodes
                   << ",\"largest_tree\":" << largest_tree << ",\"elapsed_seconds\":" << seconds << "}\n";
        for (const auto &entry : categories) {
            std::cout << entry.first;
            for (const auto &category : entry.second) std::cout << ' ' << category.first << '=' << category.second;
            std::cout << '\n';
        }
        std::cout << "Outputs: " << fs::absolute(output) << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
