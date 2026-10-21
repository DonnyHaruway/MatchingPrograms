#include "iterated_da_comparison.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

std::uint64_t parse_integer(const std::string &option, const std::string &value)
{
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument(option + " requires non-negative integers");
    }
    return std::stoull(value);
}

std::vector<int> parse_list(const std::string &option, const std::string &value)
{
    std::vector<int> values;
    std::size_t start = 0;
    while (true) {
        const auto comma = value.find(',', start);
        const auto number = parse_integer(option, value.substr(start, comma == std::string::npos ? comma : comma - start));
        if (number > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument(option + " value is too large");
        }
        values.push_back(static_cast<int>(number));
        if (comma == std::string::npos) return values;
        start = comma + 1;
    }
}

void help()
{
    std::cout
        << "Usage: compare_iterated_da_5x8 [options]\n"
        << "Compare random / history-priority / firm-preference iterated DA on both proposing sides.\n\n"
        << "  --trials N                 Profiles per K, shared by all 6 algorithms (default: 10000)\n"
        << "  --conflict-pairs K[,K...]   Exact unordered rejection-pair counts, 0..28 (default: 7)\n"
        << "                             Use all to test every K from 0 through 28\n"
        << "  --capacities C,C,C,C,C     Firm capacities (default: 2,2,2,2,2)\n"
        << "  --seed N                   Reproducible base seed (default: 20261014)\n"
        << "  --colleague-mode directed|symmetric (default: directed)\n"
        << "  --firm-scores linear|superincreasing (default: linear)\n"
        << "  --output DIR               Output root (default: experiments/stability_simulation/outputs/iterated_da_5x8)\n"
        << "  --summary-only             Skip per-profile JSONL records\n"
        << "  --help, -h                 Show help\n";
}

} // namespace

int main(int argc, char **argv)
{
    try {
        iterated_da_comparison::Config config;
        std::filesystem::path output = "experiments/stability_simulation/outputs/iterated_da_5x8";
        for (int index = 1; index < argc; ++index) {
            const std::string option = argv[index];
            if (option == "--help" || option == "-h") {
                help();
                return 0;
            }
            if (option == "--summary-only") {
                config.record_instances = false;
                continue;
            }
            if (option != "--trials" && option != "--conflict-pairs" && option != "--capacities"
                && option != "--seed" && option != "--colleague-mode" && option != "--firm-scores"
                && option != "--output") throw std::invalid_argument("unknown option: " + option);
            if (index + 1 >= argc) throw std::invalid_argument("missing value after " + option);
            const std::string value = argv[++index];
            if (option == "--trials") config.trials = parse_integer(option, value);
            else if (option == "--seed") config.seed = parse_integer(option, value);
            else if (option == "--capacities") config.capacities = parse_list(option, value);
            else if (option == "--conflict-pairs") {
                if (value == "all") {
                    config.conflict_pair_counts.resize(iterated_da_comparison::MAX_CONFLICT_PAIRS + 1);
                    std::iota(config.conflict_pair_counts.begin(), config.conflict_pair_counts.end(), 0);
                } else config.conflict_pair_counts = parse_list(option, value);
            } else if (option == "--colleague-mode") config.colleague_mode = value;
            else if (option == "--firm-scores") config.firm_scores = value;
            else output = value;
        }
        iterated_da_comparison::validate_config(config);
#ifdef _OPENMP
        // Tiny 5x8 profiles: avoid spawning parallel score loops for every matching.
        omp_set_num_threads(1);
#endif
        const auto directory = stability_simulation::create_run_directory(output, config.seed, config.firm_scores);
        std::cout << "Output directory: " << directory << '\n' << std::flush;
        iterated_da_comparison::run_experiment(config, directory);
        std::cout << "Comparison completed: " << directory << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
