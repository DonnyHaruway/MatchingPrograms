#include "simulation_common.hpp"

#include "MatchingSystem.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    using namespace stability_simulation;

    require(
        enumerate_assignment_vectors(3, {2, 1}).size() == 19,
        "2x3 must have exactly 19 capacity-feasible matchings");
    require(
        enumerate_assignment_vectors(5, {2, 2, 1}).size() == 456,
        "3x5 must have exactly 456 capacity-feasible matchings");

    std::set<std::string> exhaustive_profiles;
    for (std::uint64_t index = 0; index < 18432; ++index) {
        exhaustive_profiles.insert(
            instance_fingerprint(make_exhaustive_2x3_instance(index)));
    }
    require(
        exhaustive_profiles.size() == 18432,
        "2x3 generator must enumerate exactly 18,432 distinct profiles");

    const auto random_a = make_random_3x5_instance(DEFAULT_SEED, 17);
    const auto random_b = make_random_3x5_instance(DEFAULT_SEED, 17);
    require(
        instance_fingerprint(random_a) == instance_fingerprint(random_b),
        "same seed and trial must reproduce the 3x5 profile");

    MatchingSystem system(5, 3);
    system.set_prefs(
        random_a.worker_prefs, random_a.firm_prefs, random_a.colleague_prefs);
    system.set_firm_capacities({2, 2, 1});
    const auto algorithm_seed = doctor_priority_seed(DEFAULT_SEED, "3x5", 17);
    const auto first = system.run_algorithm("doctor_dictator", algorithm_seed);
    const auto second = system.run_algorithm("doctor_dictator", algorithm_seed);
    require(first == second, "seeded doctor_dictator output must be reproducible");

    auto superincreasing = random_a;
    apply_firm_score_mode(superincreasing, "superincreasing");
    for (std::size_t firm = 0; firm < random_a.firm_prefs.size(); ++firm) {
        auto sorted = superincreasing.firm_prefs[firm];
        std::sort(sorted.begin(), sorted.end());
        int lower_sum = 0;
        for (int score : sorted) {
            require(score > lower_sum, "firm scores must be superincreasing");
            lower_sum += score;
        }
        for (std::size_t a = 0; a < random_a.firm_prefs[firm].size(); ++a) {
            for (std::size_t b = 0; b < random_a.firm_prefs[firm].size(); ++b) {
                require(
                    (random_a.firm_prefs[firm][a] < random_a.firm_prefs[firm][b])
                        == (superincreasing.firm_prefs[firm][a]
                            < superincreasing.firm_prefs[firm][b]),
                    "superincreasing transform must preserve firm rankings");
            }
        }
    }
    require(
        instance_fingerprint(superincreasing) != instance_fingerprint(random_a),
        "superincreasing transform must change 3x5 firm scores");

    std::cout << "simulation_common checks passed\n";
    return 0;
}
