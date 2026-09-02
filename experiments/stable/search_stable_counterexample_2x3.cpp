#include "CommonTypes.hpp"
#include "MatchingSystem.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using MatchingTypes::INF;
using MatchingTypes::UNMATCHED;

namespace {

constexpr int N_WORKERS = 3;
constexpr int N_FIRMS = 2;
const std::vector<int> CAPACITIES = {2, 1};

struct Options {
    std::uint64_t trials = 100000;
    unsigned int seed = 20260828U;
    int infeasible_pair_count = -1;
    bool exhaustive = false;
};

struct Instance {
    std::vector<std::vector<int>> worker_prefs;
    std::vector<std::vector<int>> firm_prefs;
    std::vector<std::vector<int>> colleague_prefs;
};

enum class BlockType {
    type1,
    type2,
};

struct BlockingPair {
    BlockType type;
    int firm;
    int worker;
    int replaced_worker = UNMATCHED;
};

struct Evaluation {
    bool feasible = true;
    bool individually_rational = true;
    std::optional<BlockingPair> blocking_pair;

    bool stable() const
    {
        return feasible && individually_rational && !blocking_pair.has_value();
    }
};

struct SearchResult {
    bool found = false;
    std::uint64_t trial = 0;
    Instance instance;
    std::vector<Matching> matchings;
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
                << "Usage: search_stable_counterexample_2x3 [options]\n\n"
                << "Options:\n"
                << "  --trials N    Number of random instances (default: 100000)\n"
                << "  --seed N      Random seed (default: 20260828)\n"
                << "  --infeasible-pairs N\n"
                << "                Fix the number of infeasible pairs to 0..3\n"
                << "                (default: choose each pair independently)\n"
                << "  --exhaustive  Enumerate every preference profile in the domain\n"
                << "  --help, -h    Show this help\n";
            std::exit(0);
        }
        if (arg == "--exhaustive") {
            options.exhaustive = true;
            continue;
        }
        if (arg == "--trials" || arg == "--seed" || arg == "--infeasible-pairs") {
            if (i + 1 >= argc) throw std::invalid_argument("Missing value after " + arg);
            const auto parsed = parse_u64(arg, argv[++i]);
            if (arg == "--trials") options.trials = parsed;
            if (arg == "--seed") options.seed = static_cast<unsigned int>(parsed);
            if (arg == "--infeasible-pairs") {
                options.infeasible_pair_count = static_cast<int>(parsed);
            }
            continue;
        }
        throw std::invalid_argument("Unknown option: " + arg);
    }
    if (options.trials == 0) throw std::invalid_argument("--trials must be positive");
    if (options.infeasible_pair_count > 3) {
        throw std::invalid_argument("--infeasible-pairs must be between 0 and 3");
    }
    return options;
}

std::vector<int> scores_from_strict_order(
    const std::vector<int> &best_to_worst,
    int outside_option,
    int real_alternative_count)
{
    const auto outside = std::find(best_to_worst.begin(), best_to_worst.end(), outside_option);
    const int outside_rank = static_cast<int>(std::distance(best_to_worst.begin(), outside));
    std::vector<int> scores(real_alternative_count, 0);
    for (int rank = 0; rank < static_cast<int>(best_to_worst.size()); ++rank) {
        const int alternative = best_to_worst[rank];
        if (alternative != outside_option) scores[alternative] = outside_rank - rank;
    }
    return scores;
}

std::vector<std::vector<int>> random_colleague_prefs(
    std::mt19937 &rng,
    int infeasible_pair_count)
{
    std::vector<std::vector<int>> prefs(N_WORKERS, std::vector<int>(N_WORKERS, 0));
    std::vector<std::pair<int, int>> pairs = {{0, 1}, {0, 2}, {1, 2}};

    if (infeasible_pair_count >= 0) {
        std::shuffle(pairs.begin(), pairs.end(), rng);
        for (int index = 0; index < infeasible_pair_count; ++index) {
            const auto [left, right] = pairs[index];
            prefs[left][right] = -INF;
            prefs[right][left] = -INF;
        }
    } else {
        std::bernoulli_distribution infeasible(0.5);
        for (const auto &[left, right] : pairs) {
            if (!infeasible(rng)) continue;
            prefs[left][right] = -INF;
            prefs[right][left] = -INF;
        }
    }
    return prefs;
}

Instance make_random_instance(std::mt19937 &rng, int infeasible_pair_count)
{
    Instance instance;

    for (int worker = 0; worker < N_WORKERS; ++worker) {
        std::vector<int> order = {0, 1, N_FIRMS};
        std::shuffle(order.begin(), order.end(), rng);
        instance.worker_prefs.push_back(
            scores_from_strict_order(order, N_FIRMS, N_FIRMS));
    }

    for (int firm = 0; firm < N_FIRMS; ++firm) {
        std::vector<int> order = {0, 1, 2, N_WORKERS};
        std::shuffle(order.begin(), order.end(), rng);
        instance.firm_prefs.push_back(
            scores_from_strict_order(order, N_WORKERS, N_WORKERS));
    }

    instance.colleague_prefs = random_colleague_prefs(rng, infeasible_pair_count);
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

Evaluation evaluate_matching(const Matching &matching, const Instance &instance)
{
    Evaluation evaluation;
    const auto &worker_matches = matching.get_agent_matchs();
    const auto &firm_matches = matching.get_firm_matchs();

    for (int firm = 0; firm < N_FIRMS; ++firm) {
        if (!is_feasible_set(firm_matches[firm], instance)) {
            evaluation.feasible = false;
            return evaluation;
        }
    }

    for (int worker = 0; worker < N_WORKERS; ++worker) {
        const int firm = worker_matches[worker];
        if (firm == UNMATCHED) continue;
        if (instance.worker_prefs[worker][firm] <= 0
            || instance.firm_prefs[firm][worker] <= 0) {
            evaluation.individually_rational = false;
            return evaluation;
        }
    }

    for (int firm = 0; firm < N_FIRMS; ++firm) {
        const auto &assigned = firm_matches[firm];
        for (int worker = 0; worker < N_WORKERS; ++worker) {
            if (worker_matches[worker] == firm) continue;

            const int current_firm = worker_matches[worker];
            const int current_score = current_firm == UNMATCHED
                ? 0
                : instance.worker_prefs[worker][current_firm];
            if (instance.worker_prefs[worker][firm] <= current_score) continue;

            if (static_cast<int>(assigned.size()) < CAPACITIES[firm]
                && instance.firm_prefs[firm][worker] > 0) {
                std::set<int> after = assigned;
                after.insert(worker);
                if (is_feasible_set(after, instance)) {
                    evaluation.blocking_pair = BlockingPair{
                        BlockType::type1, firm, worker, UNMATCHED};
                    return evaluation;
                }
            }

            for (int incumbent : assigned) {
                if (instance.firm_prefs[firm][worker]
                    <= instance.firm_prefs[firm][incumbent]) {
                    continue;
                }
                std::set<int> after = assigned;
                after.erase(incumbent);
                after.insert(worker);
                if (is_feasible_set(after, instance)) {
                    evaluation.blocking_pair = BlockingPair{
                        BlockType::type2, firm, worker, incumbent};
                    return evaluation;
                }
            }
        }
    }
    return evaluation;
}

std::vector<Matching> enumerate_matchings(const Instance &instance)
{
    MatchingSystem system(N_WORKERS, N_FIRMS);
    system.set_agent_prefs(instance.worker_prefs);
    system.set_firm_prefs(instance.firm_prefs);
    system.set_agent_col_prefs(instance.colleague_prefs);
    system.set_firm_capacities(CAPACITIES);
    return system.make_all_matchings();
}

SearchResult search(const Options &options)
{
    std::mt19937 rng(options.seed);
    for (std::uint64_t trial = 1; trial <= options.trials; ++trial) {
        Instance instance = make_random_instance(rng, options.infeasible_pair_count);
        auto matchings = enumerate_matchings(instance);

        const bool stable_exists = std::any_of(
            matchings.begin(), matchings.end(), [&](const Matching &matching) {
                return evaluate_matching(matching, instance).stable();
            });
        if (!stable_exists) {
            return SearchResult{true, trial, std::move(instance), std::move(matchings)};
        }
    }
    return {};
}

std::vector<std::vector<int>> all_strict_preferences(int real_alternative_count)
{
    const int outside_option = real_alternative_count;
    std::vector<int> order(real_alternative_count + 1);
    std::iota(order.begin(), order.end(), 0);
    std::vector<std::vector<int>> preferences;
    do {
        preferences.push_back(
            scores_from_strict_order(order, outside_option, real_alternative_count));
    } while (std::next_permutation(order.begin(), order.end()));
    return preferences;
}

std::vector<std::vector<std::vector<int>>> all_colleague_preferences(int pair_count)
{
    std::vector<std::vector<std::vector<int>>> result;
    for (unsigned int mask = 0; mask < 8U; ++mask) {
        if (pair_count >= 0
            && __builtin_popcount(mask) != pair_count) {
            continue;
        }
        std::vector<std::vector<int>> prefs(
            N_WORKERS, std::vector<int>(N_WORKERS, 0));
        unsigned int bit = 0;
        for (int left = 0; left < N_WORKERS; ++left) {
            for (int right = left + 1; right < N_WORKERS; ++right) {
                if ((mask & (1U << bit)) != 0U) {
                    prefs[left][right] = -INF;
                    prefs[right][left] = -INF;
                }
                ++bit;
            }
        }
        result.push_back(std::move(prefs));
    }
    return result;
}

SearchResult search_exhaustively(const Options &options)
{
    const auto worker_preferences = all_strict_preferences(N_FIRMS);
    const auto firm_preferences = all_strict_preferences(N_WORKERS);
    const auto colleague_preferences =
        all_colleague_preferences(options.infeasible_pair_count);
    std::uint64_t tested = 0;

    for (const auto &w1 : worker_preferences) {
        for (const auto &w2 : worker_preferences) {
            for (const auto &w3 : worker_preferences) {
                const std::vector<std::vector<int>> workers = {w1, w2, w3};
                for (const auto &f1 : firm_preferences) {
                    for (const auto &f2 : firm_preferences) {
                        const std::vector<std::vector<int>> firms = {f1, f2};
                        for (const auto &colleagues : colleague_preferences) {
                            Instance instance{workers, firms, colleagues};
                            auto matchings = enumerate_matchings(instance);
                            ++tested;
                            const bool stable_exists = std::any_of(
                                matchings.begin(), matchings.end(),
                                [&](const Matching &matching) {
                                    return evaluate_matching(matching, instance).stable();
                                });
                            if (!stable_exists) {
                                return SearchResult{
                                    true, tested, std::move(instance), std::move(matchings)};
                            }
                        }
                    }
                }
            }
        }
    }
    SearchResult result;
    result.trial = tested;
    return result;
}

std::string alternative_name(int alternative, const std::string &prefix, int outside_option)
{
    if (alternative == outside_option) return "emptyset";
    return prefix + std::to_string(alternative + 1);
}

void print_order(
    const std::vector<int> &scores,
    const std::string &prefix,
    int outside_option)
{
    std::vector<int> order(scores.size() + 1);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        const int lhs_score = lhs == outside_option ? 0 : scores[lhs];
        const int rhs_score = rhs == outside_option ? 0 : scores[rhs];
        return lhs_score > rhs_score;
    });
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (i != 0) std::cout << " > ";
        std::cout << alternative_name(order[i], prefix, outside_option);
    }
}

void print_instance(const Instance &instance)
{
    std::cout << "capacities: q_f1=2, q_f2=1\n";
    std::cout << "worker preferences (all workers):\n";
    for (int worker = 0; worker < N_WORKERS; ++worker) {
        std::cout << "  w" << worker + 1 << ": ";
        print_order(instance.worker_prefs[worker], "f", N_FIRMS);
        std::cout << "\n";
    }
    std::cout << "firm preferences (all firms):\n";
    for (int firm = 0; firm < N_FIRMS; ++firm) {
        std::cout << "  f" << firm + 1 << ": ";
        print_order(instance.firm_prefs[firm], "w", N_WORKERS);
        std::cout << "\n";
    }

    std::cout << "infeasible pairs B: {";
    bool first = true;
    for (int left = 0; left < N_WORKERS; ++left) {
        for (int right = left + 1; right < N_WORKERS; ++right) {
            if (instance.colleague_prefs[left][right] != -INF) continue;
            if (!first) std::cout << ", ";
            std::cout << "{w" << left + 1 << ",w" << right + 1 << "}";
            first = false;
        }
    }
    std::cout << "}\n";
}

void print_matching(const Matching &matching)
{
    const auto &firm_matches = matching.get_firm_matchs();
    const auto &worker_matches = matching.get_agent_matchs();
    std::cout << "mu={";
    for (int firm = 0; firm < N_FIRMS; ++firm) {
        if (firm != 0) std::cout << ", ";
        std::cout << "f" << firm + 1 << ":{";
        bool first = true;
        for (int worker : firm_matches[firm]) {
            if (!first) std::cout << ',';
            std::cout << "w" << worker + 1;
            first = false;
        }
        std::cout << "}";
    }
    std::cout << ", unmatched:{";
    bool first = true;
    for (int worker = 0; worker < N_WORKERS; ++worker) {
        if (worker_matches[worker] != UNMATCHED) continue;
        if (!first) std::cout << ',';
        std::cout << "w" << worker + 1;
        first = false;
    }
    std::cout << "}}";
}

void print_diagnostics(const SearchResult &result)
{
    std::size_t feasible_ir_count = 0;
    std::cout << "feasible and individually rational matchings:\n";
    for (const auto &matching : result.matchings) {
        const Evaluation evaluation = evaluate_matching(matching, result.instance);
        if (!evaluation.feasible || !evaluation.individually_rational) continue;
        ++feasible_ir_count;
        std::cout << "  ";
        print_matching(matching);
        if (!evaluation.blocking_pair.has_value()) {
            std::cout << " -> STABLE (unexpected)\n";
            continue;
        }

        const auto &block = *evaluation.blocking_pair;
        std::cout << " -> type-" << (block.type == BlockType::type1 ? "1" : "2")
                  << " block (f" << block.firm + 1 << ",w" << block.worker + 1 << ")";
        if (block.type == BlockType::type2) {
            std::cout << ", replacing w" << block.replaced_worker + 1;
        }
        std::cout << "\n";
    }
    std::cout << "feasible and individually rational matching count: "
              << feasible_ir_count << "\n";
}

} // namespace

int main(int argc, char **argv)
{
    try {
        const Options options = parse_options(argc, argv);
        const auto start = std::chrono::steady_clock::now();

        std::cout << "Stable-matching counterexample search\n"
                  << "workers: 3, firms: 2, capacities: [2,1]\n"
                  << "mode: " << (options.exhaustive ? "exhaustive" : "random") << "\n";
        if (!options.exhaustive) {
            std::cout << "trials: " << options.trials << ", seed: " << options.seed << "\n";
        }
        std::cout
                  << "infeasible pair count: ";
        if (options.infeasible_pair_count < 0) {
            std::cout << "random (independent for each pair)\n";
        } else {
            std::cout << options.infeasible_pair_count << "\n";
        }

        const SearchResult result = options.exhaustive
            ? search_exhaustively(options)
            : search(options);
        if (result.found) {
            std::cout << "Counterexample found at "
                      << (options.exhaustive ? "instance " : "trial ")
                      << result.trial << ".\n";
            print_instance(result.instance);
            print_diagnostics(result);
        } else if (options.exhaustive) {
            std::cout << "No counterexample found in exhaustive search of "
                      << result.trial << " instances.\n";
        } else {
            std::cout << "No counterexample found in " << options.trials << " trials.\n";
        }

        const auto end = std::chrono::steady_clock::now();
        std::cout << std::fixed << std::setprecision(3)
                  << "elapsed: "
                  << std::chrono::duration<double>(end - start).count()
                  << " seconds\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
