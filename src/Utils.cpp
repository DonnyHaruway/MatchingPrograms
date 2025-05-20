#include "Utils.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

std::vector<int> generate_random_ranked(int size, std::mt19937& rng) {
    std::vector<int> order(size);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    return order;
}

std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng) {
    if (min_val > max_val) {
        throw std::invalid_argument("min_val must not be greater than max_val");
    }

    std::uniform_int_distribution<> dist(min_val, max_val);
    std::vector<int> v(size);
    for (int& x : v) x = dist(rng);
    return v;
}

std::vector<std::vector<int>> generate_combinations(const std::vector<int>& set, int k) {
    std::vector<std::vector<int>> result;

    std::vector<bool> bitmask(set.size(), false);
    std::fill(bitmask.end() - k, bitmask.end(), true);  // k個だけtrue（選ばれる）

    do {
        std::vector<int> comb;
        for (size_t i = 0; i < set.size(); ++i) {
            if (bitmask[i]) comb.push_back(set[i]);
        }
        result.push_back(comb);
    } while (std::next_permutation(bitmask.begin(), bitmask.end()));

    return result;
}

std::map<int, std::vector<std::vector<int>>> generate_all_subsets_by_size(
    const std::vector<int>& set,
    int max_size
) {
    std::map<int, std::vector<std::vector<int>>> all_combinations;

    for (int k = 0; k <= max_size; ++k) {
        all_combinations[k] = generate_combinations(set, k);
    }

    return all_combinations;
}

void generate_matchings_recursive(
    int firm_idx,
    const std::vector<std::vector<std::vector<int>>>& all_candidates,
    std::vector<std::pair<int, std::vector<int>>>& current_matching,
    std::set<int>& used_agents,
    std::vector<std::vector<std::pair<int, std::vector<int>>>>& result
) {
    // ベースケース：すべての企業に割り当て終わったら結果に追加
    if (firm_idx == all_candidates.size()) {
        result.push_back(current_matching);
        return;
    }

    // 候補をすべて試す
    for (const auto& group : all_candidates[firm_idx]) {
        bool valid = true;

        // 重複agentチェック
        for (int agent : group) {
            if (used_agents.count(agent)) {
                valid = false;
                break;
            }
        }

        if (!valid) continue;

        // 割り当てを追加
        for (int agent : group) used_agents.insert(agent);
        current_matching.emplace_back(firm_idx, group);

        // 再帰的に次の企業へ
        generate_matchings_recursive(firm_idx + 1, all_candidates, current_matching, used_agents, result);

        // バックトラック
        current_matching.pop_back();
        for (int agent : group) used_agents.erase(agent);
    }
}