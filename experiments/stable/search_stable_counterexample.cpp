#include "CommonTypes.hpp"
#include "MatchingSystem.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using MatchingTypes::INF;

namespace {

constexpr int N_AGENTS = 3;
constexpr int N_FIRMS = 1;

struct Options {
    std::uint64_t trials = 100000;
    unsigned int seed = 20260828U;
    int capacity = 2;
    bool exhaustive = false;
};

struct Instance {
    std::vector<std::vector<int>> agent_prefs;
    std::vector<std::vector<int>> firm_prefs;
    std::vector<std::vector<int>> colleague_prefs;
};

struct CheckResult {
    bool stable_exists = false;
    std::size_t matching_count = 0;
    std::size_t stable_matching_count = 0;
};

std::uint64_t parse_u64(const std::string &name, const std::string &value)
{
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed);
    if (consumed != value.size()) {
        throw std::invalid_argument(name + " must be an integer: " + value);
    }
    return parsed;
}

Options parse_options(int argc, char **argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: search_stable_counterexample [options]\n\n"
                << "Options:\n"
                << "  --trials N      Number of random instances (default: 100000)\n"
                << "  --seed N        Random seed (default: 20260828)\n"
                << "  --capacity N    Capacity of the single firm, 1..3 (default: 2)\n"
                << "  --exhaustive    Enumerate all 1536 instances\n"
                << "  --help, -h      Show this help\n";
            std::exit(0);
        }
        if (arg == "--exhaustive") {
            options.exhaustive = true;
            continue;
        }
        if (arg == "--trials" || arg == "--seed" || arg == "--capacity") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value after " + arg);
            }
            const std::string value = argv[++i];
            const auto parsed = parse_u64(arg, value);
            if (arg == "--trials") options.trials = parsed;
            if (arg == "--seed") options.seed = static_cast<unsigned int>(parsed);
            if (arg == "--capacity") options.capacity = static_cast<int>(parsed);
            continue;
        }
        throw std::invalid_argument("Unknown option: " + arg);
    }

    if (options.trials == 0) {
        throw std::invalid_argument("--trials must be positive");
    }
    if (options.capacity < 1 || options.capacity > N_AGENTS) {
        throw std::invalid_argument("--capacity must be between 1 and 3");
    }
    return options;
}

std::vector<std::vector<int>> colleague_prefs_from_mask(unsigned int mask)
{
    std::vector<std::vector<int>> prefs(N_AGENTS, std::vector<int>(N_AGENTS, 0));
    unsigned int bit = 0;
    for (int i = 0; i < N_AGENTS; ++i) {
        for (int j = i + 1; j < N_AGENTS; ++j) {
            if ((mask & (1U << bit)) != 0U) {
                prefs[i][j] = -INF;
                prefs[j][i] = -INF;
            }
            ++bit;
        }
    }
    return prefs;
}

Instance make_random_instance(std::mt19937 &rng)
{
    Instance instance;

    // 企業が1社なので、各労働者の選好は f1 > emptyset または emptyset > f1。
    std::bernoulli_distribution acceptable(0.5);
    instance.agent_prefs.assign(N_AGENTS, std::vector<int>(N_FIRMS, 0));
    for (int worker = 0; worker < N_AGENTS; ++worker) {
        instance.agent_prefs[worker][0] = acceptable(rng) ? 1 : -1;
    }

    // w1, w2, w3, emptyset の一様ランダムな厳密順位を、emptyset=0 のスコアに変換する。
    std::vector<int> firm_order = {0, 1, 2, N_AGENTS};
    std::shuffle(firm_order.begin(), firm_order.end(), rng);
    const auto empty_position = std::find(firm_order.begin(), firm_order.end(), N_AGENTS);
    const int empty_rank = static_cast<int>(std::distance(firm_order.begin(), empty_position));
    std::vector<int> firm_scores(N_AGENTS, 0);
    for (int rank = 0; rank < static_cast<int>(firm_order.size()); ++rank) {
        const int alternative = firm_order[rank];
        if (alternative != N_AGENTS) firm_scores[alternative] = empty_rank - rank;
    }
    instance.firm_prefs.push_back(firm_scores);

    std::uniform_int_distribution<unsigned int> mask_dist(0U, (1U << 3U) - 1U);
    instance.colleague_prefs = colleague_prefs_from_mask(mask_dist(rng));
    return instance;
}

bool is_feasible_set(const std::set<int> &workers, const Instance &instance)
{
    for (auto left = workers.begin(); left != workers.end(); ++left) {
        for (auto right = std::next(left); right != workers.end(); ++right) {
            if (instance.colleague_prefs[*left][*right] == -INF) return false;
        }
    }
    return true;
}

bool is_stable_for_infeasible_pairs(
    const Matching &matching,
    const Instance &instance,
    int capacity)
{
    const auto &assigned = matching.get_firm_matchs().front();

    // 安定性を判定する対象は実現可能かつ個人合理的なマッチングに限る。
    if (!is_feasible_set(assigned, instance)) return false;
    for (int worker : assigned) {
        if (instance.agent_prefs[worker][0] <= 0) return false;
        if (instance.firm_prefs[0][worker] <= 0) return false;
    }

    for (int candidate = 0; candidate < N_AGENTS; ++candidate) {
        if (assigned.count(candidate) != 0) continue;

        // 企業が1社なので、候補者が現在よりf1を好む条件は f1 > emptyset と同値。
        if (instance.agent_prefs[candidate][0] <= 0) continue;

        // Type 1: 空きがあり、候補者を追加しても実現可能で、企業も候補者を受容する。
        if (static_cast<int>(assigned.size()) < capacity
            && instance.firm_prefs[0][candidate] > 0) {
            std::set<int> after = assigned;
            after.insert(candidate);
            if (is_feasible_set(after, instance)) return false;
        }

        // Type 2: 既存の労働者1人と、企業がより好む候補者を交換できる。
        for (int incumbent : assigned) {
            if (instance.firm_prefs[0][candidate] <= instance.firm_prefs[0][incumbent]) {
                continue;
            }
            std::set<int> after = assigned;
            after.erase(incumbent);
            after.insert(candidate);
            if (is_feasible_set(after, instance)) return false;
        }
    }
    return true;
}

CheckResult check_instance(const Instance &instance, int capacity)
{
    MatchingSystem system(N_AGENTS, N_FIRMS);
    system.set_agent_prefs(instance.agent_prefs);
    system.set_firm_prefs(instance.firm_prefs);
    system.set_agent_col_prefs(instance.colleague_prefs);
    system.set_firm_capacities({capacity});

    auto matchings = system.make_all_matchings();
    CheckResult result;
    result.matching_count = matchings.size();

    for (const auto &matching : matchings) {
        if (is_stable_for_infeasible_pairs(matching, instance, capacity)) {
            result.stable_exists = true;
            ++result.stable_matching_count;
        }
    }
    return result;
}

std::string instance_signature(const Instance &instance)
{
    std::ostringstream out;
    for (const auto &pref : instance.agent_prefs) out << pref[0] << ',';
    out << '|';
    for (int score : instance.firm_prefs.front()) out << score << ',';
    out << '|';
    for (int i = 0; i < N_AGENTS; ++i) {
        for (int j = i + 1; j < N_AGENTS; ++j) {
            out << (instance.colleague_prefs[i][j] == -INF ? '1' : '0');
        }
    }
    return out.str();
}

void print_instance(const Instance &instance, int capacity)
{
    std::cout << "capacity: q1=" << capacity << "\n";
    std::cout << "worker preferences:\n";
    for (int w = 0; w < N_AGENTS; ++w) {
        std::cout << "  w" << w + 1 << ": "
                  << (instance.agent_prefs[w][0] > 0
                      ? "f1 > emptyset"
                      : "emptyset > f1")
                  << "\n";
    }

    std::vector<int> order = {0, 1, 2, N_AGENTS};
    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        const int lhs_score = lhs == N_AGENTS ? 0 : instance.firm_prefs[0][lhs];
        const int rhs_score = rhs == N_AGENTS ? 0 : instance.firm_prefs[0][rhs];
        return lhs_score > rhs_score;
    });
    std::cout << "firm preference: ";
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (i != 0) std::cout << " > ";
        if (order[i] == N_AGENTS) {
            std::cout << "emptyset";
        } else {
            std::cout << "w" << order[i] + 1;
        }
    }
    std::cout << "\n";

    std::cout << "infeasible pairs B: {";
    bool first = true;
    for (int i = 0; i < N_AGENTS; ++i) {
        for (int j = i + 1; j < N_AGENTS; ++j) {
            if (instance.colleague_prefs[i][j] != -INF) continue;
            if (!first) std::cout << ", ";
            std::cout << "{w" << i + 1 << ",w" << j + 1 << "}";
            first = false;
        }
    }
    std::cout << "}\n";
}

bool run_random_search(const Options &options)
{
    std::mt19937 rng(options.seed);
    std::set<std::string> distinct_instances;
    std::size_t matchings_per_instance = 0;

    for (std::uint64_t trial = 1; trial <= options.trials; ++trial) {
        const Instance instance = make_random_instance(rng);
        distinct_instances.insert(instance_signature(instance));
        const CheckResult result = check_instance(instance, options.capacity);
        matchings_per_instance = result.matching_count;

        if (!result.stable_exists) {
            std::cout << "Counterexample found at trial " << trial << ".\n";
            print_instance(instance, options.capacity);
            return true;
        }
    }

    std::cout << "No counterexample found.\n"
              << "tested random instances: " << options.trials << "\n"
              << "distinct instances observed: " << distinct_instances.size() << " / 1536\n"
              << "capacity-feasible assignments enumerated per instance: "
              << matchings_per_instance << "\n";
    return false;
}

bool run_exhaustive_search(const Options &options)
{
    std::vector<int> firm_order = {0, 1, 2, N_AGENTS};
    std::uint64_t tested = 0;

    do {
        const auto empty_position = std::find(firm_order.begin(), firm_order.end(), N_AGENTS);
        const int empty_rank = static_cast<int>(std::distance(firm_order.begin(), empty_position));
        std::vector<int> firm_scores(N_AGENTS, 0);
        for (int rank = 0; rank < static_cast<int>(firm_order.size()); ++rank) {
            const int alternative = firm_order[rank];
            if (alternative != N_AGENTS) firm_scores[alternative] = empty_rank - rank;
        }

        for (unsigned int worker_mask = 0; worker_mask < 8U; ++worker_mask) {
            std::vector<std::vector<int>> agent_prefs(N_AGENTS, std::vector<int>(1, 0));
            for (int worker = 0; worker < N_AGENTS; ++worker) {
                agent_prefs[worker][0] = (worker_mask & (1U << worker)) != 0U ? 1 : -1;
            }

            for (unsigned int pair_mask = 0; pair_mask < 8U; ++pair_mask) {
                const Instance instance{
                    agent_prefs,
                    {firm_scores},
                    colleague_prefs_from_mask(pair_mask),
                };
                ++tested;
                const CheckResult result = check_instance(instance, options.capacity);
                if (!result.stable_exists) {
                    std::cout << "Counterexample found in exhaustive search at instance "
                              << tested << ".\n";
                    print_instance(instance, options.capacity);
                    return true;
                }
            }
        }
    } while (std::next_permutation(firm_order.begin(), firm_order.end()));

    std::cout << "No counterexample found.\n"
              << "exhaustively tested instances: " << tested << " / 1536\n";
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    try {
        const Options options = parse_options(argc, argv);
        const auto start = std::chrono::steady_clock::now();

        std::cout << "Stable-matching counterexample search\n"
                  << "workers: " << N_AGENTS << ", firms: " << N_FIRMS
                  << ", capacity: " << options.capacity << "\n"
                  << "colleague preferences: symmetric binary values {0, -INF}\n"
                  << "mode: " << (options.exhaustive ? "exhaustive" : "random") << "\n";
        if (!options.exhaustive) {
            std::cout << "trials: " << options.trials << ", seed: " << options.seed << "\n";
        }

        const bool found = options.exhaustive
            ? run_exhaustive_search(options)
            : run_random_search(options);

        const auto end = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(end - start).count();
        std::cout << std::fixed << std::setprecision(3)
                  << "elapsed: " << elapsed << " seconds\n";
        (void)found;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
