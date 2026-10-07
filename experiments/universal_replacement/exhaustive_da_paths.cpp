#include "choice_search.hpp"
#include "problem_5x8.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

using namespace universal_replacement_experiment;
using MatchingTypes::UNMATCHED;
namespace fs = std::filesystem;

namespace {
struct Outcome {
    std::size_t id;
    bool stable;
    int matched;
    std::vector<std::pair<int, int>> blockers;
    std::vector<std::uint64_t> path_ids;
};

struct Result {
    ChoiceSearchStats stats;
    std::map<std::vector<int>, Outcome> outcomes;
    std::uint64_t stable_paths = 0;
    std::size_t stable_outcomes = 0;
    std::size_t full_stable_outcomes = 0;
    double seconds = 0;
};

fs::path create_run_directory(const fs::path &root, const std::string &prefix = "da_paths_")
{
    fs::create_directories(root);
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string name = prefix + std::to_string(stamp);
    for (std::uint64_t suffix = 0; ; ++suffix) {
        const auto path = root / (name + (suffix ? "_" + std::to_string(suffix) : ""));
        if (fs::create_directory(path)) return path;
    }
}

void write_assignment(std::ostream &out, const std::vector<int> &assignment)
{
    out << '[';
    for (std::size_t index = 0; index < assignment.size(); ++index) {
        if (index) out << ',';
        out << (assignment[index] == UNMATCHED ? 0 : assignment[index] + 1);
    }
    out << ']';
}

void write_conflict(std::ostream &out, const DAConflict &pair)
{
    out << "{\"firm\":" << pair.firm + 1 << ",\"left\":" << pair.left + 1
        << ",\"right\":" << pair.right + 1 << '}';
}

void write_blockers(std::ostream &out, const std::vector<std::pair<int, int>> &pairs)
{
    out << '[';
    for (std::size_t index = 0; index < pairs.size(); ++index) {
        if (index) out << ',';
        out << "{\"firm\":" << pairs[index].second + 1 << ",\"worker\":" << pairs[index].first + 1 << '}';
    }
    out << ']';
}

void write_problem(const fs::path &path, const Problem &problem)
{
    std::ofstream out(path);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    const auto vector_json = [&](const std::vector<int> &values) {
        out << '[';
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index) out << ',';
            out << values[index];
        }
        out << ']';
    };
    const auto matrix_json = [&](const Rankings &matrix) {
        out << '[';
        for (std::size_t row = 0; row < matrix.size(); ++row) {
            if (row) out << ',';
            vector_json(matrix[row]);
        }
        out << ']';
    };
    out << "{\"n_firms\":5,\"n_workers\":8,\"capacities\":";
    vector_json(problem.capacities);
    out << ",\"agent_prefs\":";
    matrix_json(problem.agent_prefs);
    out << ",\"firm_prefs\":";
    matrix_json(problem.firm_prefs);
    out << ",\"colleague_prefs\":";
    matrix_json(problem.colleague_prefs);
    out << ",\"worker_rankings_one_based\":";
    matrix_json(problem.worker_rankings);
    out << ",\"firm_rankings_one_based\":";
    matrix_json(problem.firm_rankings);
    out << "}\n";
}

Result investigate(const std::string &algorithm, const Problem &problem, const fs::path &directory)
{
    std::ofstream paths(directory / (algorithm + "_paths.jsonl"));
    paths.exceptions(std::ios::failbit | std::ios::badbit);
    std::ofstream cases(directory / (algorithm + "_paths.csv"));
    cases.exceptions(std::ios::failbit | std::ios::badbit);
    cases << "path_id,outcome_id,stable,matched_workers,deletions,w1,w2,w3,w4,w5,w6,w7,w8,blocking_pairs\n";
    Result result;
    const auto started = std::chrono::steady_clock::now();
    const auto runner = [&](const DARandomChoiceSelector &selector) {
        if (algorithm == "iterated_doctor") {
            return iterated_doctor_proposing_DA_algorithm(
                8, 5, problem.capacities, problem.agent_prefs,
                problem.firm_prefs, problem.colleague_prefs, 0, selector);
        }
        if (algorithm == "iterated_firm") {
            return iterated_firm_proposing_DA_algorithm(
                8, 5, problem.capacities, problem.agent_prefs,
                problem.firm_prefs, problem.colleague_prefs, 0, selector);
        }
        return algorithm == "firm"
            ? modified_firm_proposing_DA_algorithm(8, 5, problem.capacities, problem.agent_prefs,
                                                  problem.firm_prefs, problem.colleague_prefs, 0, selector)
            : modified_worker_proposing_DA_algorithm(8, 5, problem.capacities, problem.agent_prefs,
                                                    problem.firm_prefs, problem.colleague_prefs, 0, selector);
    };
    result.stats = enumerate_choice_paths(runner, [&](std::uint64_t id, Matching &matching,
                                                    const std::vector<ChoiceDecision> &decisions) {
        const auto &assignment = matching.get_agent_matchs();
        const bool stable = matching.is_universal_replacement_stable(
            problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, problem.capacities);
        const auto blockers = matching.universal_replacement_blocking_pairs(
            problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs, problem.capacities);
        if (!matching.is_individually_rational(problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs)
            || stable != blockers.empty()) throw std::logic_error("final stability consistency failure");
        int matched = 0;
        for (int firm : assignment) if (firm != UNMATCHED) ++matched;
        auto found = result.outcomes.find(assignment);
        if (found == result.outcomes.end()) {
            const auto outcome_id = result.outcomes.size() + 1;
            found = result.outcomes.emplace(assignment, Outcome{outcome_id, stable, matched, blockers, {}}).first;
            if (stable) {
                ++result.stable_outcomes;
                if (matched == 8) ++result.full_stable_outcomes;
            }
        }
        found->second.path_ids.push_back(id);
        result.stable_paths += stable;
        paths << "{\"path_id\":" << id << ",\"algorithm\":\"" << algorithm << "\",\"outcome_id\":"
              << found->second.id << ",\"stable\":" << (stable ? "true" : "false")
              << ",\"matched_workers\":" << matched << ",\"assignment\":";
        write_assignment(paths, assignment);
        paths << ",\"blocking_pairs\":";
        write_blockers(paths, blockers);
        paths << ",\"decisions\":[";
        std::ostringstream deletion_text;
        for (std::size_t index = 0; index < decisions.size(); ++index) {
            if (index) paths << ',';
            const auto &decision = decisions[index];
            const auto &choice = decision.choice;
            const bool removal = choice.kind == DAChoiceKind::removal_endpoint;
            paths << "{\"round\":" << choice.round << ",\"kind\":\""
                  << (removal ? "removal_endpoint" : "pair_order")
                  << "\",\"option_count\":" << choice.option_count << ",\"selected_index\":"
                  << decision.selected << ",\"assignment_before\":";
            write_assignment(paths, choice.agent_match);
            paths << ",\"pairs\":[";
            for (std::size_t pair = 0; pair < choice.conflicts.size(); ++pair) {
                if (pair) paths << ',';
                write_conflict(paths, choice.conflicts[pair]);
            }
            paths << ']';
            if (removal) {
                const auto &pair = choice.conflicts.front();
                const int removed = decision.selected == 0 ? pair.left : pair.right;
                paths << ",\"removed_worker\":" << removed + 1 << ",\"firm\":" << pair.firm + 1;
                if (deletion_text.tellp() > 0) deletion_text << ';';
                deletion_text << "r" << choice.round << ":f" << pair.firm + 1 << ":w" << removed + 1;
            }
            paths << '}';
        }
        paths << "]}\n";
        cases << id << ',' << found->second.id << ',' << (stable ? "true" : "false") << ','
              << matched << ',' << deletion_text.str();
        for (int firm : assignment) {
            if (firm == UNMATCHED) cases << ",unmatched";
            else cases << ",f" << firm + 1;
        }
        cases << ',';
        for (std::size_t pair = 0; pair < blockers.size(); ++pair) {
            if (pair) cases << ';';
            cases << "f" << blockers[pair].second + 1 << ":w" << blockers[pair].first + 1;
        }
        cases << '\n';
    });
    paths.close();
    cases.close();
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return result;
}

void write_result(std::ostream &out, const std::string &algorithm, const Result &result)
{
    out << "\n" << algorithm << " proposing\n"
        << "Complete choice-path enumeration: YES (all random decisions)\n"
        << "Paths: " << result.stats.completed_paths << '\n'
        << "Stable paths: " << result.stable_paths << '\n'
        << "Unstable paths: " << result.stats.completed_paths - result.stable_paths << '\n'
        << "Distinct final matchings: " << result.outcomes.size() << '\n'
        << "Distinct stable matchings: " << result.stable_outcomes << '\n'
        << "Distinct full stable matchings: " << result.full_stable_outcomes << '\n'
        << "Replay calls: " << result.stats.replay_calls << '\n'
        << "Maximum random decisions per path: " << result.stats.max_decisions << '\n'
        << "Maximum simultaneous conflicts: " << result.stats.max_simultaneous_conflicts << '\n'
        << "Seconds (enumeration, classification and output): " << std::fixed << std::setprecision(6)
        << result.seconds << '\n'
        << "Final assignment vectors are in worker order w1..w8; 0=unmatched, 1..5=firm labels.\n";
    for (const auto &[assignment, outcome] : result.outcomes) {
        out << "Outcome " << outcome.id << ": ";
        write_assignment(out, assignment);
        out << " stable=" << (outcome.stable ? "YES" : "NO") << " paths=";
        for (std::size_t index = 0; index < outcome.path_ids.size(); ++index) {
            if (index) out << ',';
            out << outcome.path_ids[index];
        }
        out << " blockers=";
        write_blockers(out, outcome.blockers);
        out << '\n';
    }
}
} // namespace

int main(int argc, char **argv)
{
    try {
        fs::path output_root = "experiments/universal_replacement/outputs";
        std::string iterated_side;
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Usage: exhaustive_da_paths_5x8 [--iterated-doctor | --iterated-firm] [--output DIR]\n";
            return 0;
        }
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--iterated-doctor" || argument == "--iterated-firm") {
                if (!iterated_side.empty()) throw std::invalid_argument("select only one iterated proposing side");
                iterated_side = argument == "--iterated-doctor" ? "doctor" : "firm";
            }
            else if (argument == "--output" && index + 1 < argc) output_root = argv[++index];
            else throw std::invalid_argument("expected --iterated-doctor, --iterated-firm, --output DIR or --help");
        }
        const auto problem = screenshot_problem_5x8();
        const auto directory = create_run_directory(output_root,
            !iterated_side.empty() ? "iterated_" + iterated_side + "_paths_" : "da_paths_");
        write_problem(directory / "problem.json", problem);
        if (!iterated_side.empty()) {
            const auto result = investigate("iterated_" + iterated_side, problem, directory);
            std::ostringstream summary;
            summary << "Fixed 5-firm/8-worker profile in problem_5x8.hpp\n"
                    << "Complete " << iterated_side << " DA per A; select ONE conflict and ONE endpoint per B.\n"
                    << "Deletions accumulate in firm preferences; each DA restarts from empty matching.\n"
                    << "Stability is evaluated against ORIGINAL preferences. Vacancy blocking is not included.\n";
            write_result(summary, "iterated " + iterated_side, result);
            summary << "\nHas NO stable path: " << (result.stable_paths == 0 ? "YES" : "NO") << '\n'
                    << "Has AT LEAST ONE stable path: " << (result.stable_paths > 0 ? "YES" : "NO") << '\n'
                    << "Stable on EVERY path: "
                    << (result.stable_paths == result.stats.completed_paths ? "YES" : "NO") << '\n'
                    << "Counts of paths are not probability estimates.\n";
            std::ofstream out(directory / "summary.txt");
            out.exceptions(std::ios::failbit | std::ios::badbit);
            out << summary.str();
            out.close();
            std::cout << summary.str() << "Outputs: " << fs::absolute(directory) << '\n';
            return 0;
        }
        const auto firm = investigate("firm", problem, directory);
        const auto worker = investigate("worker", problem, directory);
        const bool hypothesis = worker.stable_paths == 0 && firm.stable_paths > 0;
        std::ostringstream summary;
        summary << "Fixed 5-firm/8-worker profile in problem_5x8.hpp\n"
                << "Deletion permanent; proposal history retained; one simultaneous proposal round per A.\n"
                << "Stability is evaluated against ORIGINAL preferences, including individual rationality and feasibility.\n"
                << "Vacancy blocking is not included. Path IDs are per algorithm, starting from 1.\n";
        write_result(summary, "firm", firm);
        write_result(summary, "doctor (worker)", worker);
        summary << "\nHypothesis: worker has NO stable path and firm has AT LEAST ONE stable path.\n"
                << "Hypothesis holds for this profile and implementation: " << (hypothesis ? "YES" : "NO") << '\n'
                << "Worker has no stable path: " << (worker.stable_paths == 0 ? "YES" : "NO") << '\n'
                << "Firm has at least one stable path: " << (firm.stable_paths > 0 ? "YES" : "NO") << '\n'
                << "Firm is stable on every path: " << (firm.stable_paths == firm.stats.completed_paths ? "YES" : "NO") << '\n'
                << "Counts of paths are not probability estimates.\n";
        std::ofstream out(directory / "summary.txt");
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << summary.str();
        out.close();
        std::cout << summary.str() << "Outputs: " << fs::absolute(directory) << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
