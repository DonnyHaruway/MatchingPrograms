#pragma
#include <vector>
#include <random>
#include <map>
#include <set>

/// @brief make the ranking to opponents
/// @param size size of the opponents
/// @param rng random number generator
/// @return the ranking (vector of integers)
std::vector<int> generate_random_ranked(int size, std::mt19937& rng);

/// @brief make the preference numbers to opponents
/// @param size size of opponents
/// @param min_val minimum value of preference
/// @param max_val maximum value of preference
/// @param rng random number generator
/// @return the preference numbers (vector of integers)
std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng);

/// @brief generate combinations of a set by size
/// @param set the set of integers
/// @param k size of combinations
/// @return all combinations of the set 
std::vector<std::vector<int>> generate_combinations(const std::vector<int>& set, int k);

/// @brief generate all subsets of a set by size
/// @param set the set of integers
/// @param max_size maximum size of subsets
/// @return map of all subsets by size
std::map<int, std::vector<std::vector<int>>> generate_all_subsets_by_size(
    const std::vector<int>& set,
    int max_size
);

/// @brief generate matchings recursively
/// @param firm_idx index of the firm
/// @param all_candidates all candidates for the firm
/// @param current_matching current matching
/// @param used_agents used agents
/// @param result matching result
void generate_matchings_recursive(
    int firm_idx,
    const std::vector<std::vector<std::vector<int>>>& all_candidates,
    std::vector<std::pair<int, std::vector<int>>>& current_matching,
    std::set<int>& used_agents,
    std::vector<std::vector<std::pair<int, std::vector<int>>>>& result
);