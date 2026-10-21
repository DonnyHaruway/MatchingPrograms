#pragma once

#include "simulation_common.hpp"
#include "algorithms.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace iterated_da_comparison {

constexpr int N_FIRMS = 5;
constexpr int N_WORKERS = 8;
constexpr int MAX_CONFLICT_PAIRS = N_WORKERS * (N_WORKERS - 1) / 2;

struct Config {
    std::uint64_t trials = 10000;
    std::uint64_t seed = 20261014;
    std::vector<int> capacities = {2, 2, 2, 2, 2};
    std::vector<int> conflict_pair_counts = {7};
    std::string colleague_mode = "directed";
    std::string firm_scores = "linear";
    bool record_instances = true;
};

struct AlgorithmSpec {
    std::string name;
    std::string proposing_side;
    std::string policy;
    decltype(&iterated_doctor_proposing_DA_algorithm) run;
};

struct Outcome {
    std::vector<int> agent_match;
    std::uint32_t algorithm_seed;
    int initial_conflict_pairs;
    bool weakly_stable;
    bool universal_replacement_stable;
    double algorithm_seconds;
};

struct Statistics {
    std::uint64_t profiles = 0;
    std::uint64_t weakly_stable = 0;
    std::uint64_t universal_replacement_stable = 0;
    std::uint64_t both_stable = 0;
    std::uint64_t matched_workers = 0;
    std::uint64_t initial_conflict_pairs = 0;
    double algorithm_seconds = 0;

    void add(const Outcome &outcome);
};

void validate_config(const Config &config);
const std::vector<AlgorithmSpec> &algorithms();

// Rankings are identical for the same (seed, trial) across K. Rejection sets
// are nested prefixes of a uniform random order of the 28 unordered pairs.
stability_simulation::Instance make_instance(const Config &config, std::uint64_t trial, int k);
int count_rejection_pairs(const std::vector<std::vector<int>> &colleague_prefs);
int count_matching_conflicts(const Matching &matching, const std::vector<std::vector<int>> &colleague_prefs);
std::vector<Outcome> evaluate_profile(
    const Config &config, const stability_simulation::Instance &instance, std::uint64_t trial);

// One row per (K, algorithm), plus detailed results and initial-DA strata on disk.
std::vector<std::vector<Statistics>> run_experiment(
    const Config &config, const std::filesystem::path &run_directory);

} // namespace iterated_da_comparison
