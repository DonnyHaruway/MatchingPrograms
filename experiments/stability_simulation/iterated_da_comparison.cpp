#include "iterated_da_comparison.hpp"

#include "CommonTypes.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>

namespace iterated_da_comparison {
namespace {

using MatchingTypes::INF;
using MatchingTypes::UNMATCHED;

std::uint64_t mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::vector<int> strict_scores(int count, std::mt19937_64 &rng)
{
    std::vector<int> scores(count);
    std::iota(scores.begin(), scores.end(), 1);
    std::shuffle(scores.begin(), scores.end(), rng);
    return scores;
}

template <typename T>
void json_vector(std::ostream &out, const std::vector<T> &values)
{
    out << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) out << ',';
        out << values[index];
    }
    out << ']';
}

void json_matrix(std::ostream &out, const std::vector<std::vector<int>> &matrix)
{
    out << '[';
    for (std::size_t index = 0; index < matrix.size(); ++index) {
        if (index != 0) out << ',';
        json_vector(out, matrix[index]);
    }
    out << ']';
}

double rate(std::uint64_t count, std::uint64_t total)
{
    return total == 0 ? 0.0 : static_cast<double>(count) / static_cast<double>(total);
}

void csv_header(std::ostream &out, bool stratified)
{
    out << "conflict_pairs,conflict_pair_density,algorithm,proposing_side,policy";
    if (stratified) out << ",initial_da_conflict_pairs";
    out << ",profiles,weakly_stable_count,weakly_stable_rate,universal_replacement_stable_count,"
        << "universal_replacement_stable_rate,both_stable_count,both_stable_rate,"
        << "mean_matched_workers,mean_initial_da_conflict_pairs,algorithm_seconds,"
        << "seed,capacities,colleague_mode,firm_scores,vacancy_blocking\n";
}

void csv_row(std::ostream &out, const Config &config, int k, const AlgorithmSpec &algorithm,
             const Statistics &stats, int initial_count = -1)
{
    out << std::setprecision(12) << k << ',' << rate(k, MAX_CONFLICT_PAIRS)
        << ',' << algorithm.name << ',' << algorithm.proposing_side << ',' << algorithm.policy;
    if (initial_count >= 0) out << ',' << initial_count;
    out << ',' << stats.profiles << ',' << stats.weakly_stable << ',' << rate(stats.weakly_stable, stats.profiles)
        << ',' << stats.universal_replacement_stable << ',' << rate(stats.universal_replacement_stable, stats.profiles)
        << ',' << stats.both_stable << ',' << rate(stats.both_stable, stats.profiles)
        << ',' << rate(stats.matched_workers, stats.profiles)
        << ',' << rate(stats.initial_conflict_pairs, stats.profiles)
        << ',' << stats.algorithm_seconds << ',' << config.seed << ",\"";
    json_vector(out, config.capacities);
    out << "\"," << config.colleague_mode << ',' << config.firm_scores << ",true\n";
}

void write_instance(std::ostream &out, const Config &config, int k, std::uint64_t trial,
                    const stability_simulation::Instance &instance, const std::vector<Outcome> &outcomes)
{
    out << "{\"trial\":" << trial << ",\"seed\":" << config.seed
        << ",\"n_firms\":5,\"n_workers\":8,\"vacancy_blocking\":true,\"conflict_pairs\":" << k
        << ",\"colleague_mode\":\"" << config.colleague_mode
        << "\",\"firm_scores\":\"" << config.firm_scores << "\",\"capacities\":";
    json_vector(out, config.capacities);
    out << ",\"worker_prefs\":";
    json_matrix(out, instance.worker_prefs);
    out << ",\"firm_prefs\":";
    json_matrix(out, instance.firm_prefs);
    out << ",\"colleague_prefs\":";
    json_matrix(out, instance.colleague_prefs);
    out << ",\"algorithms\":{";
    // Each profile stays on a single JSONL line.
    for (std::size_t index = 0; index < outcomes.size(); ++index) {
        if (index != 0) out << ',';
        const auto &result = outcomes[index];
        out << '"' << algorithms()[index].name << "\":{\"algorithm_seed\":" << result.algorithm_seed
            << ",\"initial_da_conflict_pairs\":" << result.initial_conflict_pairs << ",\"agent_match\":";
        json_vector(out, result.agent_match);
        out << ",\"is_weakly_stable\":" << (result.weakly_stable ? "true" : "false")
            << ",\"is_universal_replacement_stable\":" << (result.universal_replacement_stable ? "true" : "false")
            << ",\"algorithm_seconds\":" << std::setprecision(12) << result.algorithm_seconds << '}';
    }
    out << "}}\n";
}

void write_summary(std::ostream &out, const Config &config, int k,
                   const std::vector<Statistics> &summary, double elapsed_seconds)
{
    out << "Iterated DA comparison: firms=5, workers=8\ncapacities=";
    json_vector(out, config.capacities);
    out << "\nprofiles=" << config.trials << "\nseed=" << config.seed
        << "\nconflict_pairs=" << k << '/' << MAX_CONFLICT_PAIRS
        << "\ncolleague_mode=" << config.colleague_mode << "\nfirm_scores=" << config.firm_scores
        << "\nvacancy_blocking=true\nAll algorithms receive the same profiles; stability uses original preferences.\n"
        << "K counts unordered rejection pairs across all workers, not co-located pairs.\n"
        << "No enumeration of stable matchings or conditioning on existence is performed.\n\n";
    out << std::fixed << std::setprecision(2);
    for (std::size_t index = 0; index < summary.size(); ++index) {
        const auto &stats = summary[index];
        out << algorithms()[index].name << '\n'
            << "  weakly_stable: " << stats.weakly_stable << '/' << stats.profiles
            << " (" << 100 * rate(stats.weakly_stable, stats.profiles) << "%)\n"
            << "  universal_replacement_stable: " << stats.universal_replacement_stable << '/' << stats.profiles
            << " (" << 100 * rate(stats.universal_replacement_stable, stats.profiles) << "%)\n"
            << "  both_stable: " << stats.both_stable << '/' << stats.profiles
            << " (" << 100 * rate(stats.both_stable, stats.profiles) << "%)\n"
            << "  mean_matched_workers: " << rate(stats.matched_workers, stats.profiles) << '\n'
            << "  mean_initial_da_conflict_pairs: " << rate(stats.initial_conflict_pairs, stats.profiles) << '\n';
    }
    out << "\nelapsed_seconds=" << elapsed_seconds << '\n';
}

} // namespace

void Statistics::add(const Outcome &outcome)
{
    ++profiles;
    weakly_stable += outcome.weakly_stable;
    universal_replacement_stable += outcome.universal_replacement_stable;
    both_stable += outcome.weakly_stable && outcome.universal_replacement_stable;
    matched_workers += std::count_if(outcome.agent_match.begin(), outcome.agent_match.end(),
                                    [](int firm) { return firm != UNMATCHED; });
    initial_conflict_pairs += outcome.initial_conflict_pairs;
    algorithm_seconds += outcome.algorithm_seconds;
}

void validate_config(const Config &config)
{
    if (config.trials == 0) throw std::invalid_argument("--trials must be positive");
    if (config.capacities.size() != N_FIRMS
        || std::any_of(config.capacities.begin(), config.capacities.end(), [](int cap) { return cap < 0; })) {
        throw std::invalid_argument("--capacities requires 5 non-negative integers");
    }
    if (config.conflict_pair_counts.empty()) throw std::invalid_argument("--conflict-pairs cannot be empty");
    std::set<int> seen;
    for (int k : config.conflict_pair_counts) {
        if (k < 0 || k > MAX_CONFLICT_PAIRS) throw std::invalid_argument("--conflict-pairs must be in 0..28");
        if (!seen.insert(k).second) throw std::invalid_argument("--conflict-pairs must not contain duplicates");
    }
    if (config.colleague_mode != "directed" && config.colleague_mode != "symmetric") {
        throw std::invalid_argument("--colleague-mode must be directed or symmetric");
    }
    if (!stability_simulation::is_valid_firm_score_mode(config.firm_scores)) {
        throw std::invalid_argument("--firm-scores must be linear or superincreasing");
    }
}

const std::vector<AlgorithmSpec> &algorithms()
{
    static const std::vector<AlgorithmSpec> specs = {
        {"iterated_doctor_propose_DA", "doctor", "random", iterated_doctor_proposing_DA_algorithm},
        {"iterated_doctor_propose_DA_history_priority", "doctor", "history_priority", iterated_doctor_proposing_DA_history_priority_algorithm},
        {"iterated_doctor_propose_DA_firm_preference", "doctor", "firm_preference", iterated_doctor_proposing_DA_firm_preference_algorithm},
        {"iterated_firm_propose_DA", "firm", "random", iterated_firm_proposing_DA_algorithm},
        {"iterated_firm_propose_DA_history_priority", "firm", "history_priority", iterated_firm_proposing_DA_history_priority_algorithm},
        {"iterated_firm_propose_DA_firm_preference", "firm", "firm_preference", iterated_firm_proposing_DA_firm_preference_algorithm}
    };
    return specs;
}

stability_simulation::Instance make_instance(const Config &config, std::uint64_t trial, int k)
{
    if (k < 0 || k > MAX_CONFLICT_PAIRS) throw std::invalid_argument("conflict count must be in 0..28");
    if (config.colleague_mode != "directed" && config.colleague_mode != "symmetric") {
        throw std::invalid_argument("invalid colleague mode");
    }
    const auto trial_seed = mix(config.seed ^ mix(trial));
    std::mt19937_64 rank_rng(mix(trial_seed ^ 0x52414e4bULL));
    std::mt19937_64 colleague_rng(mix(trial_seed ^ 0x434f4c4cULL));
    stability_simulation::Instance instance;
    for (int worker = 0; worker < N_WORKERS; ++worker) instance.worker_prefs.push_back(strict_scores(N_FIRMS, rank_rng));
    for (int firm = 0; firm < N_FIRMS; ++firm) instance.firm_prefs.push_back(strict_scores(N_WORKERS, rank_rng));
    stability_simulation::apply_firm_score_mode(instance, config.firm_scores);
    instance.colleague_prefs.assign(N_WORKERS, std::vector<int>(N_WORKERS, 0));
    std::vector<std::pair<int, int>> pairs;
    for (int left = 0; left < N_WORKERS; ++left) {
        for (int right = left + 1; right < N_WORKERS; ++right) pairs.emplace_back(left, right);
    }
    std::shuffle(pairs.begin(), pairs.end(), colleague_rng);
    std::uniform_int_distribution<int> direction(0, 2);
    for (int index = 0; index < k; ++index) {
        const auto [left, right] = pairs[index];
        const int mode = direction(colleague_rng);
        if (config.colleague_mode == "symmetric" || mode == 0 || mode == 2) instance.colleague_prefs[left][right] = -INF;
        if (config.colleague_mode == "symmetric" || mode == 1 || mode == 2) instance.colleague_prefs[right][left] = -INF;
    }
    return instance;
}

int count_rejection_pairs(const std::vector<std::vector<int>> &colleague_prefs)
{
    int count = 0;
    for (std::size_t left = 0; left < colleague_prefs.size(); ++left) {
        for (std::size_t right = left + 1; right < colleague_prefs.size(); ++right) {
            count += colleague_prefs[left][right] == -INF || colleague_prefs[right][left] == -INF;
        }
    }
    return count;
}

int count_matching_conflicts(const Matching &matching, const std::vector<std::vector<int>> &colleague_prefs)
{
    int count = 0;
    for (const auto &workers : matching.get_firm_matchs()) {
        for (auto left = workers.begin(); left != workers.end(); ++left) {
            for (auto right = std::next(left); right != workers.end(); ++right) {
                count += colleague_prefs[*left][*right] == -INF || colleague_prefs[*right][*left] == -INF;
            }
        }
    }
    return count;
}

std::vector<Outcome> evaluate_profile(const Config &config, const stability_simulation::Instance &instance, std::uint64_t trial)
{
    const auto doctor_da = doctor_proposing_DA_algorithm(N_WORKERS, N_FIRMS, config.capacities, instance.worker_prefs, instance.firm_prefs);
    const auto firm_da = firm_proposing_DA_algorithm(N_WORKERS, N_FIRMS, config.capacities, instance.worker_prefs, instance.firm_prefs);
    const int doctor_conflicts = count_matching_conflicts(doctor_da, instance.colleague_prefs);
    const int firm_conflicts = count_matching_conflicts(firm_da, instance.colleague_prefs);
    std::vector<Outcome> outcomes;
    for (const auto &algorithm : algorithms()) {
        const bool doctor = algorithm.proposing_side == "doctor";
        // Same seed across policies on a proposing side; independent of K and execution order.
        const auto seed = static_cast<std::uint32_t>(mix(config.seed ^ mix(trial) ^ (doctor ? 0x444f4354ULL : 0x4649524dULL)));
        const auto start = std::chrono::steady_clock::now();
        auto matching = algorithm.run(N_WORKERS, N_FIRMS, config.capacities, instance.worker_prefs,
                                      instance.firm_prefs, instance.colleague_prefs, seed, {});
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (count_matching_conflicts(matching, instance.colleague_prefs) != 0) {
            throw std::logic_error(algorithm.name + " returned colleague conflicts");
        }
        for (int firm = 0; firm < N_FIRMS; ++firm) {
            if (static_cast<int>(matching.get_firm_matchs()[firm].size()) > config.capacities[firm]) {
                throw std::logic_error(algorithm.name + " exceeded capacity");
            }
        }
        // Use original preferences, never the internally reduced firm preferences.
        const bool weak = matching.is_weakly_stable(instance.worker_prefs, instance.firm_prefs, instance.colleague_prefs, config.capacities);
        const bool universal = matching.is_universal_replacement_stable(instance.worker_prefs, instance.firm_prefs, instance.colleague_prefs, config.capacities);
        outcomes.push_back({matching.get_agent_matchs(), seed, doctor ? doctor_conflicts : firm_conflicts, weak, universal, seconds});
    }
    return outcomes;
}

std::vector<std::vector<Statistics>> run_experiment(const Config &config, const std::filesystem::path &run_directory)
{
    validate_config(config);
    std::filesystem::create_directories(run_directory);
    std::ofstream combined(run_directory / "summary.csv");
    combined.exceptions(std::ios::badbit | std::ios::failbit);
    csv_header(combined, false);
    std::vector<std::vector<Statistics>> all_summaries;
    for (int k : config.conflict_pair_counts) {
        const auto directory = run_directory / ("k" + std::to_string(k));
        std::filesystem::create_directories(directory);
        std::ofstream instances;
        if (config.record_instances) {
            instances.open(directory / "instances.jsonl");
            instances.exceptions(std::ios::badbit | std::ios::failbit);
        }
        std::vector<Statistics> summary(algorithms().size());
        std::vector<std::map<int, Statistics>> by_initial_conflicts(algorithms().size());
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t trial = 0; trial < config.trials; ++trial) {
            const auto instance = make_instance(config, trial, k);
            if (count_rejection_pairs(instance.colleague_prefs) != k) throw std::logic_error("generator did not produce exact K");
            const auto outcomes = evaluate_profile(config, instance, trial);
            for (std::size_t index = 0; index < outcomes.size(); ++index) {
                summary[index].add(outcomes[index]);
                by_initial_conflicts[index][outcomes[index].initial_conflict_pairs].add(outcomes[index]);
            }
            if (config.record_instances) write_instance(instances, config, k, trial, instance, outcomes);
            if ((trial + 1) % 1000 == 0) std::cout << "K=" << k << ": " << trial + 1 << '/' << config.trials << " profiles\n" << std::flush;
        }
        if (config.record_instances) instances.close();
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::ofstream csv(directory / "summary.csv"), strata(directory / "by_initial_conflicts.csv"), text(directory / "summary.txt");
        csv.exceptions(std::ios::badbit | std::ios::failbit);
        strata.exceptions(std::ios::badbit | std::ios::failbit);
        text.exceptions(std::ios::badbit | std::ios::failbit);
        csv_header(csv, false);
        csv_header(strata, true);
        for (std::size_t index = 0; index < summary.size(); ++index) {
            csv_row(csv, config, k, algorithms()[index], summary[index]);
            csv_row(combined, config, k, algorithms()[index], summary[index]);
            for (const auto &[initial, stats] : by_initial_conflicts[index]) csv_row(strata, config, k, algorithms()[index], stats, initial);
        }
        write_summary(text, config, k, summary, elapsed);
        write_summary(std::cout, config, k, summary, elapsed);
        csv.close();
        strata.close();
        text.close();
        all_summaries.push_back(summary);
    }
    combined.close();
    return all_summaries;
}

} // namespace iterated_da_comparison
