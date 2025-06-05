#include "Utils.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream>

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

std::vector<std::vector<std::vector<int>>> prepare_all_candidates(
    const std::vector<int>& agent_ids,
    const std::vector<int>& firm_capacities
) {
    int max_capacity = *std::max_element(firm_capacities.begin(), firm_capacities.end());
    auto subset_map = generate_all_subsets_by_size(agent_ids, max_capacity);

    std::vector<std::vector<std::vector<int>>> all_candidates;
    for (int cap : firm_capacities)
    {
        std::vector<std::vector<int>> merged;
        for (int k = 0; k <= cap; ++k)
        {
            const auto& subsets = subset_map.at(k);
            merged.insert(merged.end(), subsets.begin(), subsets.end());
        }
        all_candidates.push_back(merged);
    }
    return all_candidates;
};

void generate_matchings_recursive(
    int firm_idx,
    const int& n_agents,
    const std::vector<std::vector<std::vector<int>>>& all_candidates,
    std::vector<std::vector<int>>& current_matching,
    std::set<int>& used_agents,
    std::vector<Matching>& result
) {
    // ベースケース：すべての企業に割り当て終わったら結果に追加
    if (firm_idx == all_candidates.size()) {
        Matching matching = Matching::from_firm_assignment(current_matching, n_agents);
        result.push_back(matching);
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
        current_matching.push_back(group);

        // std::cout << "used_agents" << "\n";
        // for (int agent : used_agents) std::cout << agent << " ";
        // std::cout << "\n\n";

        // std::cout << "current_matching" << "\n";
        // for (int i=0; i<current_matching.size(); i++) {
        //     auto match = current_matching[i];
        //     std::cout << "match" << i << "\n";
        //     for (int agent : match) std::cout << agent << " ";
        //     std::cout << "\n\n";
        // }


        // 再帰的に次の企業へ
        generate_matchings_recursive(firm_idx + 1, n_agents, all_candidates, current_matching, used_agents, result);

        // バックトラック
        current_matching.pop_back();
        for (int agent : group) used_agents.erase(agent);
    }
}

