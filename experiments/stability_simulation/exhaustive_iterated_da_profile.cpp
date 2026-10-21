#include "iterated_da_comparison.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {
using namespace iterated_da_comparison;
using MatchingTypes::UNMATCHED;

std::uint64_t integer(const std::string &value)
{
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("expected a non-negative integer: " + value);
    }
    return std::stoull(value);
}

void assignment_csv(std::ostream &out, const std::vector<int> &assignment)
{
    for (int firm : assignment) out << ',' << firm;
}

class Search {
public:
    Search(const Config &config, const stability_simulation::Instance &profile, std::ostream &full_csv)
        : config(config), profile(profile), full_csv(full_csv), assignment(N_WORKERS, UNMATCHED),
          firms(N_FIRMS), checked(N_WORKERS + 1, 0), stable(N_WORKERS + 1, 0), witness(N_WORKERS + 1) {}

    void run() { enumerate(0, 0); }

    void report(std::ostream &out, std::uint64_t trial, double seconds) const
    {
        if (checked[0] != 1) throw std::logic_error("missing empty matching");
        out << "Complete exhaustive universal-replacement stability search\n"
            << "trial=" << trial << " (zero-based)\nseed=" << config.seed
            << "\nconflict_pairs=" << config.conflict_pair_counts.front()
            << "\ncolleague_mode=" << config.colleague_mode << "\nfirm_scores=" << config.firm_scores
            << "\ncapacities=";
        for (int cap : config.capacities) out << cap << ' ';
        out << "\ncomplete_enumeration=true\nvacancy_blocking=true\n"
            << "capacity_feasible_matchings_checked=" << std::accumulate(checked.begin(), checked.end(), std::uint64_t{0})
            << "\nuniversal_replacement_stable_matchings=" << std::accumulate(stable.begin(), stable.end(), std::uint64_t{0})
            << "\nall_8_matched_stable_matchings=" << stable.back()
            << "\nall_8_matched_stable_exists=" << (stable.back() ? "true" : "false")
            << "\n\nmatched_workers,checked_count,stable_count\n";
        for (int count = 0; count <= N_WORKERS; ++count) out << count << ',' << checked[count] << ',' << stable[count] << '\n';
        out << "\nFirst stable witness at each matched count (worker order, zero-based firm IDs, -1=unmatched):\n";
        for (int count = 0; count <= N_WORKERS; ++count) {
            if (!witness[count]) continue;
            out << count;
            assignment_csv(out, *witness[count]);
            out << '\n';
        }
        out << "\nVacancy-addition blocking is included in this predicate.\n"
            << "elapsed_seconds=" << std::fixed << std::setprecision(6) << seconds << '\n';
    }

private:
    void enumerate(int worker, int matched)
    {
        if (worker == N_WORKERS) {
            ++checked[matched];
            // Every capacity-feasible assignment is checked, even if it contains
            // colleague rejections. Fresh Matching avoids cached-score reuse.
            Matching candidate(assignment, firms);
            if (!candidate.is_universal_replacement_stable(
                    profile.worker_prefs, profile.firm_prefs, profile.colleague_prefs, config.capacities)) return;
            ++stable[matched];
            if (!witness[matched]) witness[matched] = assignment;
            if (matched == N_WORKERS) {
                full_csv << stable[matched];
                assignment_csv(full_csv, assignment);
                full_csv << '\n';
            }
            return;
        }
        assignment[worker] = UNMATCHED;
        enumerate(worker + 1, matched);
        for (int firm = 0; firm < N_FIRMS; ++firm) {
            if (static_cast<int>(firms[firm].size()) >= config.capacities[firm]) continue;
            assignment[worker] = firm;
            firms[firm].insert(worker);
            enumerate(worker + 1, matched + 1);
            firms[firm].erase(worker);
        }
        assignment[worker] = UNMATCHED;
    }

    const Config &config;
    const stability_simulation::Instance &profile;
    std::ostream &full_csv;
    std::vector<int> assignment;
    std::vector<std::set<int>> firms;
    std::vector<std::uint64_t> checked, stable;
    std::vector<std::optional<std::vector<int>>> witness;
};
} // namespace

int main(int argc, char **argv)
{
    try {
        iterated_da_comparison::Config config;
        std::optional<std::uint64_t> trial;
        std::filesystem::path output = "experiments/stability_simulation/outputs/iterated_da_exhaustive";
        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--help") {
                std::cout << "Usage: exhaustive_iterated_da_profile --trial N [--seed N] [--conflict-pairs K]\n"
                             "       [--capacities C,C,C,C,C] [--colleague-mode directed|symmetric]\n"
                             "       [--firm-scores linear|superincreasing] [--output DIR]\n"
                             "Defaults match compare_iterated_da_5x8. Checks all capacity-feasible matchings without early stop.\n";
                return 0;
            }
            if (arg != "--trial" && arg != "--seed" && arg != "--conflict-pairs" && arg != "--capacities"
                && arg != "--colleague-mode" && arg != "--firm-scores" && arg != "--output") {
                throw std::invalid_argument("unknown option: " + arg);
            }
            if (index + 1 >= argc) throw std::invalid_argument("missing value after " + arg);
            const std::string value = argv[++index];
            if (arg == "--trial") trial = integer(value);
            else if (arg == "--seed") config.seed = integer(value);
            else if (arg == "--colleague-mode") config.colleague_mode = value;
            else if (arg == "--firm-scores") config.firm_scores = value;
            else if (arg == "--output") output = value;
            else if (arg == "--conflict-pairs") {
                const auto count = integer(value);
                if (count > MAX_CONFLICT_PAIRS) throw std::invalid_argument("conflict count must be in 0..28");
                config.conflict_pair_counts = {static_cast<int>(count)};
            } else {
                config.capacities.clear();
                std::size_t start = 0;
                while (true) {
                    const auto comma = value.find(',', start);
                    const auto cap = integer(value.substr(start, comma == std::string::npos ? comma : comma - start));
                    if (cap > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) throw std::invalid_argument("capacity too large");
                    config.capacities.push_back(static_cast<int>(cap));
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
        }
        if (!trial) throw std::invalid_argument("--trial is required");
        validate_config(config);
#ifdef _OPENMP
        omp_set_num_threads(1);
#endif
        const auto profile = make_instance(config, *trial, config.conflict_pair_counts.front());
        const auto directory = stability_simulation::create_run_directory(output, config.seed, config.firm_scores);
        std::cout << "Output directory: " << directory << '\n' << std::flush;
        std::ofstream fingerprint(directory / "profile_fingerprint.txt");
        fingerprint.exceptions(std::ios::badbit | std::ios::failbit);
        fingerprint << stability_simulation::instance_fingerprint(profile) << '\n';
        fingerprint.close();
        std::ofstream csv(directory / "all_8_matched_stable_matchings.csv");
        csv.exceptions(std::ios::badbit | std::ios::failbit);
        csv << "stable_id,w0,w1,w2,w3,w4,w5,w6,w7\n";
        Search search(config, profile, csv);
        const auto start = std::chrono::steady_clock::now();
        search.run();
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        csv.close();
        std::ofstream summary(directory / "summary.txt");
        summary.exceptions(std::ios::badbit | std::ios::failbit);
        search.report(summary, *trial, seconds);
        summary.close();
        search.report(std::cout, *trial, seconds);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
