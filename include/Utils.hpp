#pragma
#include <vector>
#include <random>
#include <map>
#include <set>

std::vector<int> generate_random_ranked(int size, std::mt19937& rng);
std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng);
std::vector<std::vector<int>> generate_combinations(const std::vector<int>& set, int k);
std::map<int, std::vector<std::vector<int>>> generate_all_subsets_by_size(
    const std::vector<int>& set,
    int max_size
);
void generate_matchings_recursive(
    int firm_idx,
    const std::vector<std::vector<std::vector<int>>>& all_candidates,
    std::vector<std::pair<int, std::vector<int>>>& current_matching,
    std::set<int>& used_agents,
    std::vector<std::vector<std::pair<int, std::vector<int>>>>& result
);