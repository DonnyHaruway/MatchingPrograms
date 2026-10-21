#include "iterated_da_comparison.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <set>
#include <sstream>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace iterated_da_comparison;
using MatchingTypes::INF;
using MatchingTypes::UNMATCHED;

TEST(IteratedDAComparison, GeneratesExactUnorderedCountsWithValidStrictRankings)
{
    Config config;
    for (const auto &mode : {"directed", "symmetric"}) {
        config.colleague_mode = mode;
        for (int k = 0; k <= MAX_CONFLICT_PAIRS; ++k) {
            for (std::uint64_t trial = 0; trial < 10; ++trial) {
                const auto instance = make_instance(config, trial, k);
                EXPECT_EQ(count_rejection_pairs(instance.colleague_prefs), k);
                EXPECT_EQ(instance.worker_prefs.size(), N_WORKERS);
                EXPECT_EQ(instance.firm_prefs.size(), N_FIRMS);
                for (const auto &row : instance.worker_prefs) {
                    auto sorted = row;
                    std::sort(sorted.begin(), sorted.end());
                    EXPECT_EQ(sorted, (std::vector<int>{1, 2, 3, 4, 5}));
                }
                for (const auto &row : instance.firm_prefs) {
                    auto sorted = row;
                    std::sort(sorted.begin(), sorted.end());
                    EXPECT_EQ(sorted, (std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}));
                }
                for (int left = 0; left < N_WORKERS; ++left) {
                    EXPECT_EQ(instance.colleague_prefs[left][left], 0);
                    for (int right = 0; right < N_WORKERS; ++right) {
                        EXPECT_TRUE(instance.colleague_prefs[left][right] == 0 || instance.colleague_prefs[left][right] == -INF);
                        if (config.colleague_mode == "symmetric") {
                            EXPECT_EQ(instance.colleague_prefs[left][right], instance.colleague_prefs[right][left]);
                        }
                    }
                }
            }
        }
    }
}

TEST(IteratedDAComparison, ReproducesProfilesAndKeepsRankingsAndNestedRejectionsAcrossK)
{
    Config config;
    auto previous = make_instance(config, 12, 0);
    for (int k = 1; k <= MAX_CONFLICT_PAIRS; ++k) {
        const auto next = make_instance(config, 12, k);
        EXPECT_EQ(previous.worker_prefs, next.worker_prefs);
        EXPECT_EQ(previous.firm_prefs, next.firm_prefs);
        for (int left = 0; left < N_WORKERS; ++left) {
            for (int right = 0; right < N_WORKERS; ++right) {
                if (previous.colleague_prefs[left][right] == -INF) EXPECT_EQ(next.colleague_prefs[left][right], -INF);
            }
        }
        previous = next;
    }
    EXPECT_EQ(stability_simulation::instance_fingerprint(make_instance(config, 12, 7)),
              stability_simulation::instance_fingerprint(make_instance(config, 12, 7)));
    EXPECT_NE(stability_simulation::instance_fingerprint(make_instance(config, 12, 7)),
              stability_simulation::instance_fingerprint(make_instance(config, 13, 7)));
    config.firm_scores = "superincreasing";
    const auto transformed = make_instance(config, 12, 7);
    for (auto row : transformed.firm_prefs) {
        std::sort(row.begin(), row.end());
        EXPECT_EQ(row, (std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128}));
    }
}

TEST(IteratedDAComparison, DistinguishesPotentialPairsFromCoLocatedPairs)
{
    std::vector<std::vector<int>> colleagues(N_WORKERS, std::vector<int>(N_WORKERS, 0));
    colleagues[0][1] = colleagues[1][0] = colleagues[0][2] = -INF;
    const auto matching = Matching::from_firm_assignment({{0, 1}, {2}, {3, 4}, {5, 6}, {7}}, N_WORKERS);
    EXPECT_EQ(count_rejection_pairs(colleagues), 2);
    EXPECT_EQ(count_matching_conflicts(matching, colleagues), 1);
}

TEST(IteratedDAComparison, RejectsInvalidConfiguration)
{
    Config config;
    EXPECT_NO_THROW(validate_config(config));
    for (const auto &counts : std::vector<std::vector<int>>{{}, {-1}, {29}, {7, 7}}) {
        config.conflict_pair_counts = counts;
        EXPECT_THROW(validate_config(config), std::invalid_argument);
    }
    config = Config{};
    config.trials = 0;
    EXPECT_THROW(validate_config(config), std::invalid_argument);
    config = Config{};
    config.capacities = {2, 2};
    EXPECT_THROW(validate_config(config), std::invalid_argument);
    config.capacities = {2, 2, 2, 2, -1};
    EXPECT_THROW(validate_config(config), std::invalid_argument);
    config = Config{};
    config.colleague_mode = "invalid";
    EXPECT_THROW(validate_config(config), std::invalid_argument);
    config = Config{};
    config.firm_scores = "invalid";
    EXPECT_THROW(validate_config(config), std::invalid_argument);
}

TEST(IteratedDAComparison, IndependentlyCountsBothStabilityPredicates)
{
    Statistics stats;
    stats.add({{0, 1, UNMATCHED}, 42, 2, true, false, 0});
    stats.add({{0, UNMATCHED, UNMATCHED}, 42, 1, false, true, 0});
    stats.add({{0, 1, 2}, 42, 0, true, true, 0});
    stats.add({{UNMATCHED, UNMATCHED, UNMATCHED}, 42, 3, false, false, 0});
    EXPECT_EQ(stats.profiles, 4);
    EXPECT_EQ(stats.weakly_stable, 2);
    EXPECT_EQ(stats.universal_replacement_stable, 2);
    EXPECT_EQ(stats.both_stable, 1);
    EXPECT_EQ(stats.matched_workers, 6);
    EXPECT_EQ(stats.initial_conflict_pairs, 6);
}

TEST(IteratedDAComparison, EvaluatesSixAlgorithmsReproduciblyOnOriginalPreferences)
{
#ifdef _OPENMP
    omp_set_num_threads(1);
#endif
    Config config;
    ASSERT_EQ(algorithms().size(), 6);
    for (int k : {0, 7, 28}) {
        const auto instance = make_instance(config, 17, k);
        const auto outcomes = evaluate_profile(config, instance, 17);
        const auto repeated = evaluate_profile(config, instance, 17);
        ASSERT_EQ(outcomes.size(), 6);
        for (std::size_t index = 0; index < outcomes.size(); ++index) {
            const auto &outcome = outcomes[index];
            EXPECT_EQ(outcome.agent_match, repeated[index].agent_match);
            EXPECT_EQ(outcome.algorithm_seed, repeated[index].algorithm_seed);
            EXPECT_EQ(outcome.weakly_stable, repeated[index].weakly_stable);
            EXPECT_EQ(outcome.universal_replacement_stable, repeated[index].universal_replacement_stable);
            std::vector<std::set<int>> firms(N_FIRMS);
            for (int worker = 0; worker < N_WORKERS; ++worker) {
                if (outcome.agent_match[worker] != UNMATCHED) firms[outcome.agent_match[worker]].insert(worker);
            }
            auto reconstructed = Matching::from_firm_assignment(firms, N_WORKERS);
            EXPECT_EQ(count_matching_conflicts(reconstructed, instance.colleague_prefs), 0);
            EXPECT_EQ(outcome.weakly_stable, reconstructed.is_weakly_stable(
                instance.worker_prefs, instance.firm_prefs, instance.colleague_prefs, config.capacities));
            EXPECT_EQ(outcome.universal_replacement_stable, reconstructed.is_universal_replacement_stable(
                instance.worker_prefs, instance.firm_prefs, instance.colleague_prefs, config.capacities));
            if (k == 0) {
                EXPECT_TRUE(outcome.weakly_stable);
                EXPECT_TRUE(outcome.universal_replacement_stable);
                EXPECT_EQ(outcome.initial_conflict_pairs, 0);
            }
            if (k == 28) EXPECT_TRUE(outcome.universal_replacement_stable);
        }
        // All policies share the first DA result and the seed on a proposing side.
        for (int offset : {0, 3}) {
            EXPECT_EQ(outcomes[offset].initial_conflict_pairs, outcomes[offset + 1].initial_conflict_pairs);
            EXPECT_EQ(outcomes[offset].initial_conflict_pairs, outcomes[offset + 2].initial_conflict_pairs);
            EXPECT_EQ(outcomes[offset].algorithm_seed, outcomes[offset + 1].algorithm_seed);
            EXPECT_EQ(outcomes[offset].algorithm_seed, outcomes[offset + 2].algorithm_seed);
        }
    }
}

class IteratedDAComparisonFiles : public testing::Test {
protected:
    std::filesystem::path directory;
    void SetUp() override {
#ifdef _OPENMP
        omp_set_num_threads(1);
#endif
        directory = stability_simulation::create_run_directory(std::filesystem::temp_directory_path() / "iterated_da_comparison_tests", 42);
    }
    void TearDown() override { std::filesystem::remove_all(directory); }
};

TEST_F(IteratedDAComparisonFiles, WritesPerProfileResultsAndCorrectAggregateDenominators)
{
    Config config;
    config.trials = 12;
    config.conflict_pair_counts = {0, 7};
    const auto summary = run_experiment(config, directory);
    ASSERT_EQ(summary.size(), 2);
    for (const auto &per_k : summary) {
        ASSERT_EQ(per_k.size(), 6);
        for (const auto &stats : per_k) {
            EXPECT_EQ(stats.profiles, 12);
            EXPECT_LE(stats.weakly_stable, stats.profiles);
            EXPECT_LE(stats.universal_replacement_stable, stats.profiles);
            EXPECT_LE(stats.both_stable, stats.weakly_stable);
            EXPECT_LE(stats.both_stable, stats.universal_replacement_stable);
        }
    }
    std::ifstream combined(directory / "summary.csv");
    std::string line;
    int rows = -1;
    while (std::getline(combined, line)) ++rows;
    EXPECT_EQ(rows, 12);
    for (int k : config.conflict_pair_counts) {
        const auto subdirectory = directory / ("k" + std::to_string(k));
        std::ifstream instances(subdirectory / "instances.jsonl");
        int profiles = 0;
        while (std::getline(instances, line)) {
            EXPECT_EQ(line.front(), '{');
            EXPECT_EQ(line.back(), '}');
            EXPECT_NE(line.find("\"trial\":" + std::to_string(profiles) + ','), std::string::npos);
            for (const auto &algorithm : algorithms()) EXPECT_NE(line.find('"' + algorithm.name + '"'), std::string::npos);
            ++profiles;
        }
        EXPECT_EQ(profiles, 12);
        EXPECT_TRUE(std::filesystem::exists(subdirectory / "by_initial_conflicts.csv"));
        EXPECT_TRUE(std::filesystem::exists(subdirectory / "summary.txt"));
    }
}

TEST_F(IteratedDAComparisonFiles, SummaryOnlyDoesNotWriteInstanceRecords)
{
    Config config;
    config.trials = 1;
    config.record_instances = false;
    run_experiment(config, directory);
    EXPECT_TRUE(std::filesystem::exists(directory / "k7" / "summary.csv"));
    EXPECT_FALSE(std::filesystem::exists(directory / "k7" / "instances.jsonl"));
}
