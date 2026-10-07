#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace stability_simulation {

constexpr std::uint64_t DEFAULT_SEED = 20260902ULL;
constexpr std::uint64_t DEFAULT_RANDOM_TRIALS = 18432ULL;

struct Instance {
    std::vector<std::vector<int>> worker_prefs;
    std::vector<std::vector<int>> firm_prefs;
    std::vector<std::vector<int>> colleague_prefs;
};

struct CaseSpec {
    std::string name;
    std::string mode;
    int n_firms;
    int n_workers;
    std::vector<int> capacities;
    std::uint64_t problem_count;
    std::size_t expected_matching_count;
    std::string firm_scores = "linear";
};

CaseSpec case_spec(
    const std::string &case_name,
    std::uint64_t random_trials,
    const std::string &firm_scores = "linear"
);

bool is_valid_firm_score_mode(const std::string &firm_scores);

// Converts rank scores 1..n (worst..best) in firm_prefs to 2^(rank-1),
// preserving the ordinal ranking. "linear" leaves the profile unchanged.
void apply_firm_score_mode(Instance &instance, const std::string &firm_scores);

std::vector<std::vector<int>> enumerate_assignment_vectors(
    int n_workers,
    const std::vector<int> &capacities
);

Instance make_exhaustive_2x3_instance(std::uint64_t index);
Instance make_random_3x5_instance(std::uint64_t seed, std::uint64_t trial);
std::uint32_t doctor_priority_seed(
    std::uint64_t seed,
    const std::string &case_name,
    std::uint64_t instance_index
);

std::string instance_fingerprint(const Instance &instance);

std::filesystem::path create_run_directory(
    const std::filesystem::path &output_root,
    std::uint64_t seed,
    const std::string &firm_scores = "linear"
);

void run_case(
    const CaseSpec &spec,
    std::uint64_t seed,
    const std::filesystem::path &case_output_directory
);

} // namespace stability_simulation
