#include "simulation_common.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string case_name = "all";
    std::uint64_t trials = stability_simulation::DEFAULT_RANDOM_TRIALS;
    std::uint64_t seed = stability_simulation::DEFAULT_SEED;
    std::filesystem::path output = "experiments/stability_simulation/outputs";
};

std::uint64_t parse_u64(const std::string &option, const std::string &value)
{
    if (value.empty() || value.front() == '-') {
        throw std::invalid_argument(option + " must be a non-negative integer");
    }
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed);
    if (consumed != value.size()) {
        throw std::invalid_argument(option + " must be an integer: " + value);
    }
    return parsed;
}

void print_help()
{
    std::cout
        << "Usage: simulate_matching_stability [options]\n\n"
        << "Options:\n"
        << "  --case 2x3|3x5|all  Cases to run (default: all)\n"
        << "  --trials N           Random 3x5 trials (default: 18432)\n"
        << "  --seed N             Base seed (default: 20260902)\n"
        << "  --output DIR         Output root (default: "
           "experiments/stability_simulation/outputs)\n"
        << "  --help, -h           Show this help\n";
}

Options parse_options(int argc, char **argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            std::exit(0);
        }
        if (arg != "--case" && arg != "--trials" && arg != "--seed"
            && arg != "--output") {
            throw std::invalid_argument("unknown option: " + arg);
        }
        if (i + 1 >= argc) throw std::invalid_argument("missing value after " + arg);
        const std::string value = argv[++i];
        if (arg == "--case") options.case_name = value;
        else if (arg == "--trials") options.trials = parse_u64(arg, value);
        else if (arg == "--seed") options.seed = parse_u64(arg, value);
        else options.output = value;
    }
    if (options.case_name != "2x3" && options.case_name != "3x5"
        && options.case_name != "all") {
        throw std::invalid_argument("--case must be 2x3, 3x5, or all");
    }
    if (options.trials == 0) throw std::invalid_argument("--trials must be positive");
    return options;
}

} // namespace

int main(int argc, char **argv)
{
    try {
        const Options options = parse_options(argc, argv);
        const auto run_directory = stability_simulation::create_run_directory(
            options.output, options.seed);
        std::cout << "Output directory: " << run_directory << '\n';

        std::vector<std::string> cases;
        if (options.case_name == "all") cases = {"2x3", "3x5"};
        else cases = {options.case_name};

        for (const auto &case_name : cases) {
            const auto spec = stability_simulation::case_spec(case_name, options.trials);
            stability_simulation::run_case(
                spec, options.seed, run_directory / case_name);
        }
        std::cout << "Simulation completed: " << run_directory << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
