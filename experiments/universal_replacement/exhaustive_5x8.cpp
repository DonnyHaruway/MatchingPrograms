#include "problem_5x8.hpp"
#include "Matching.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <set>
#include <string>

using namespace universal_replacement_experiment;
using MatchingTypes::UNMATCHED;
namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path output_root = "experiments/universal_replacement/outputs";
    int min_matched = 0;
};

fs::path create_run_directory(const fs::path &root)
{
    fs::create_directories(root);
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string name = "run_" + std::to_string(stamp);
    for (std::uint64_t suffix = 0; ; ++suffix) {
        const auto path = root / (name + (suffix ? "_" + std::to_string(suffix) : ""));
        if (fs::create_directory(path)) return path;
    }
}

void write_problem(std::ostream &out, const Problem &problem)
{
    out << "Fixed screenshot profile: 5 firms, 8 workers\nCapacities:";
    for (int capacity : problem.capacities) out << ' ' << capacity;
    out << "\nUnlisted alternatives are unacceptable (-INF); unmatched score = 0.\n";
    for (std::size_t firm = 0; firm < problem.firm_rankings.size(); ++firm) {
        out << "f" << firm + 1 << ':';
        for (int worker : problem.firm_rankings[firm]) out << " w" << worker;
        out << '\n';
    }
    for (std::size_t worker = 0; worker < problem.worker_rankings.size(); ++worker) {
        out << "w" << worker + 1 << ':';
        for (int firm : problem.worker_rankings[worker]) out << " f" << firm;
        out << '\n';
    }
    out << "Symmetric forbidden colleague pairs:";
    for (const auto &[left, right] : problem.forbidden_pairs) out << " {w" << left << ",w" << right << '}';
    out << '\n';
}

class ExhaustiveSearch {
public:
    ExhaustiveSearch(const Problem &problem, int min_matched, std::ostream &csv)
        : problem_(problem), min_matched_(min_matched), csv_(csv),
          assignment_(problem.worker_rankings.size(), UNMATCHED),
          firm_match_(problem.capacities.size()),
          feasible_by_size_(assignment_.size() + 1, 0),
          stable_by_size_(assignment_.size() + 1, 0),
          witnesses_(assignment_.size() + 1)
    {
        csv_ << "candidate_index,matched_workers";
        for (std::size_t worker = 0; worker < assignment_.size(); ++worker) csv_ << ",w" << worker + 1;
        csv_ << '\n';
    }

    void run() { enumerate(0, 0); }

    void report(std::ostream &out, double elapsed) const
    {
        const auto checked = std::accumulate(feasible_by_size_.begin(), feasible_by_size_.end(), std::uint64_t{0});
        const auto stable = std::accumulate(stable_by_size_.begin(), stable_by_size_.end(), std::uint64_t{0});
        if (checked == 0 || stable_by_size_[0] != 1) {
            throw std::logic_error("enumeration failed to include the unique stable empty matching");
        }
        out << "Exhaustive universal-replacement stability search\n"
            << "Complete enumeration: YES (no early stop)\n"
            << "Feasible individually rational matchings checked: " << checked << '\n'
            << "Universal-replacement stable matchings: " << stable << '\n'
            << "Minimum matched workers for existence/export: " << min_matched_ << '\n'
            << "Qualifying stable matchings: " << exported_ << '\n'
            << "Stable matching exists under this minimum: " << (exported_ ? "YES" : "NO") << '\n'
            << "All 8 workers can be matched stably: " << (stable_by_size_.back() ? "YES" : "NO") << '\n'
            << "\nmatched_workers,feasible_count,stable_count\n";
        int maximum = -1;
        for (std::size_t size = 0; size < stable_by_size_.size(); ++size) {
            out << size << ',' << feasible_by_size_[size] << ',' << stable_by_size_[size] << '\n';
            if (stable_by_size_[size]) maximum = static_cast<int>(size);
        }
        out << "Maximum number of stably matched workers: " << maximum << '\n';
        out << "\nFirst stable witness for each matched-worker count (worker order w1..w8):\n";
        for (std::size_t size = 0; size < witnesses_.size(); ++size) {
            if (!witnesses_[size]) continue;
            out << size << ':';
            for (int firm : *witnesses_[size]) {
                if (firm == UNMATCHED) out << " unmatched";
                else out << " f" << firm + 1;
            }
            out << '\n';
        }
        out << "\nVacancy addition blocking is not part of this stability definition.\n"
            << "Elapsed seconds (enumeration and CSV export): " << std::fixed << std::setprecision(6) << elapsed << '\n';
    }

private:
    void enumerate(std::size_t worker, std::size_t matched)
    {
        if (worker == assignment_.size()) {
            ++feasible_by_size_[matched];
            ++candidate_index_;
            // Fresh object for every candidate: Matching caches preference scores.
            Matching candidate(assignment_, firm_match_);
            if (!candidate.is_universal_replacement_stable(
                    problem_.agent_prefs, problem_.firm_prefs,
                    problem_.colleague_prefs, problem_.capacities)) return;
            ++stable_by_size_[matched];
            if (!witnesses_[matched]) witnesses_[matched] = assignment_;
            if (static_cast<int>(matched) < min_matched_) return;
            ++exported_;
            csv_ << candidate_index_ << ',' << matched;
            for (int firm : assignment_) {
                if (firm == UNMATCHED) csv_ << ",unmatched";
                else csv_ << ",f" << firm + 1;
            }
            csv_ << '\n';
            return;
        }

        // The unmatched branch is always included; then every acceptable firm.
        assignment_[worker] = UNMATCHED;
        enumerate(worker + 1, matched);
        for (std::size_t firm = 0; firm < firm_match_.size(); ++firm) {
            if (problem_.agent_prefs[worker][firm] < 0 || problem_.firm_prefs[firm][worker] < 0
                || static_cast<int>(firm_match_[firm].size()) >= problem_.capacities[firm]) continue;
            bool feasible = true;
            for (int incumbent : firm_match_[firm]) {
                if (problem_.colleague_prefs[worker][incumbent] == -MatchingTypes::INF
                    || problem_.colleague_prefs[incumbent][worker] == -MatchingTypes::INF) {
                    feasible = false;
                    break;
                }
            }
            if (!feasible) continue;
            assignment_[worker] = static_cast<int>(firm);
            firm_match_[firm].insert(static_cast<int>(worker));
            enumerate(worker + 1, matched + 1);
            firm_match_[firm].erase(static_cast<int>(worker));
        }
        assignment_[worker] = UNMATCHED;
    }

    const Problem &problem_;
    int min_matched_;
    std::ostream &csv_;
    std::vector<int> assignment_;
    std::vector<std::set<int>> firm_match_;
    std::vector<std::uint64_t> feasible_by_size_, stable_by_size_;
    std::vector<std::optional<std::vector<int>>> witnesses_;
    std::uint64_t candidate_index_ = 0, exported_ = 0;
};

} // namespace

int main(int argc, char **argv)
{
    try {
        Options options;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--help") {
                std::cout << "Usage: exhaustive_universal_replacement_5x8 [--output DIR] [--min-matched 0..8]\n"
                             "Enumerates the single fixed screenshot profile completely.\n"
                             "--min-matched filters existence/CSV export, not the full enumeration.\n";
                return 0;
            }
            if ((argument != "--output" && argument != "--min-matched") || index + 1 == argc) {
                throw std::invalid_argument("unknown option or missing value: " + argument);
            }
            const std::string value = argv[++index];
            if (argument == "--output") options.output_root = value;
            else {
                std::size_t consumed = 0;
                options.min_matched = std::stoi(value, &consumed);
                if (consumed != value.size() || options.min_matched < 0 || options.min_matched > 8) {
                    throw std::invalid_argument("--min-matched must be an integer in 0..8");
                }
            }
        }

        const auto problem = screenshot_problem_5x8();
        const auto directory = create_run_directory(options.output_root);
        std::ofstream definition(directory / "problem.txt");
        definition.exceptions(std::ios::failbit | std::ios::badbit);
        write_problem(definition, problem);
        definition.close();
        std::ofstream csv(directory / "stable_matchings.csv");
        csv.exceptions(std::ios::failbit | std::ios::badbit);
        ExhaustiveSearch search(problem, options.min_matched, csv);
        const auto start = std::chrono::steady_clock::now();
        search.run();
        csv.close();
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::ofstream summary(directory / "summary.txt");
        summary.exceptions(std::ios::failbit | std::ios::badbit);
        search.report(summary, elapsed);
        summary.close();
        search.report(std::cout, elapsed);
        std::cout << "Outputs: " << fs::absolute(directory) << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
