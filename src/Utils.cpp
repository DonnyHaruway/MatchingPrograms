#include "Utils.hpp"

std::vector<int> generate_random_ranked(int size, std::mt19937 &rng, std::string type, int who)
{
    std::vector<int> order(size);

    if (type == "opponent")
    {
        std::iota(order.begin(), order.end(), 1);
        std::shuffle(order.begin(), order.end(), rng);
    }
    else if (type == "col")
    {
        order[who] = 0;

        std::vector<int> others(size - 1);
        std::iota(others.begin(), others.end(), 1);
        std::shuffle(others.begin(), others.end(), rng);

        int idx = 0;
        for (int i = 0; i < size; ++i)
        {
            if (i == who)
                continue;
            order[i] = others[idx++];
        }
    }
    else
    {
        throw std::invalid_argument("Unknown type: " + type);
    }

    return order;
}

std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937 &rng, std::string type, int who)
{
    if (min_val > max_val)
    {
        throw std::invalid_argument("min_val must not be greater than max_val");
    }

    std::uniform_int_distribution<> dist(min_val, max_val);
    std::vector<int> v(size);

    if (type == "opponent")
    {
        for (int &x : v)
            x = dist(rng);
    }
    else if (type == "col")
    {
        v[who] = 0;
        for (int i = 0; i < size; ++i)
        {
            if (i != who)
            {
                v[i] = dist(rng);
            }
        }
    }
    else
    {
        throw std::invalid_argument("Unknown type: " + type);
    }
    return v;
}

std::vector<std::vector<int>> generate_combinations(const std::vector<int> &set, int k)
{
    std::vector<std::vector<int>> result;

    std::vector<bool> bitmask(set.size(), false);
    std::fill(bitmask.end() - k, bitmask.end(), true); // k個だけtrue（選ばれる）

    do
    {
        std::vector<int> comb;
        for (size_t i = 0; i < set.size(); ++i)
        {
            if (bitmask[i])
                comb.push_back(set[i]);
        }
        result.push_back(comb);
    } while (std::next_permutation(bitmask.begin(), bitmask.end()));

    return result;
}

std::map<int, std::vector<std::vector<int>>> generate_all_subsets_by_size(
    const std::vector<int> &set,
    int max_size)
{
    std::map<int, std::vector<std::vector<int>>> all_combinations;

    for (int k = 0; k <= max_size; ++k)
    {
        all_combinations[k] = generate_combinations(set, k);
    }

    return all_combinations;
}

std::vector<std::vector<std::vector<int>>> prepare_all_candidates(
    const std::vector<int> &agent_ids,
    const std::vector<int> &firm_capacities)
{
    int max_capacity = *std::max_element(firm_capacities.begin(), firm_capacities.end());
    auto subset_map = generate_all_subsets_by_size(agent_ids, max_capacity);

    std::vector<std::vector<std::vector<int>>> all_candidates;
    for (int cap : firm_capacities)
    {
        std::vector<std::vector<int>> merged;
        for (int k = 0; k <= cap; ++k)
        {
            const auto &subsets = subset_map.at(k);
            merged.insert(merged.end(), subsets.begin(), subsets.end());
        }
        all_candidates.push_back(merged);
    }
    return all_candidates;
};

void generate_matchings_recursive(
    int firm_idx,
    const int &n_agents,
    const std::vector<std::vector<std::set<int>>> &all_candidates,
    std::vector<std::set<int>> &current_matching,
    std::set<int> &used_agents,
    std::vector<Matching> &result)
{
    // ベースケース：すべての企業に割り当て終わったら結果に追加
    if (firm_idx == all_candidates.size())
    {
        Matching matching = Matching::from_firm_assignment(current_matching, n_agents);
        result.push_back(matching);
        return;
    }

    // 候補をすべて試す
    for (const auto &group : all_candidates[firm_idx])
    {
        bool valid = true;

        // 重複agentチェック
        for (int agent : group)
        {
            if (used_agents.count(agent))
            {
                valid = false;
                break;
            }
        }

        if (!valid)
            continue;

        // 割り当てを追加
        for (int agent : group)
            used_agents.insert(agent);
        current_matching.push_back(group);

        // 再帰的に次の企業へ
        generate_matchings_recursive(firm_idx + 1, n_agents, all_candidates, current_matching, used_agents, result);

        // バックトラック
        current_matching.pop_back();
        for (int agent : group)
            used_agents.erase(agent);
    }
}

int compute_firm_score(
    const std::set<int> &firm_match,
    const std::vector<int> &firm_pref)
{
    int score = 0;
    for (int agent : firm_match)
    {
        if (agent < 0 || agent >= firm_pref.size())
        {
            throw std::out_of_range("Agent index out of range in firm preferences");
        }
        score += firm_pref[agent];
    }
    return score;
}

int compute_agent_score(
    const int &firm,
    const std::set<int> &firm_match,
    const std::vector<int> &agent_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs)
{
    int score = 0;
    for (int agent_col : firm_match)
    {
        if (agent_col < 0 || agent_col >= agent_col_prefs.size())
        {
            throw std::out_of_range("Agent colleague index out of range in agent preferences");
        }
        score += agent_col_prefs[agent_col][firm];
    }
    if (firm < 0 || firm >= agent_prefs.size())
    {
        throw std::out_of_range("Firm index out of range in agent preferences");
    }
    score += agent_prefs[firm];
    return score;
}