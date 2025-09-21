#include "Utils.hpp"

std::vector<int> generate_random_ranked(int size, std::mt19937& rng, std::string type="opponent", int who=-1)
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

std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng, std::string type="opponent", int who=-1)
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
        // 自分自身は0になる
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

std::vector<std::set<int>> generate_combinations(const std::vector<int> &set, int k)
{
    std::vector<std::set<int>> result;

    std::vector<bool> bitmask(set.size(), false);
    std::fill(bitmask.end() - k, bitmask.end(), true); // k個だけtrue（選ばれる）

    do
    {
        std::set<int> comb;
        for (size_t i = 0; i < set.size(); ++i)
        {
            if (bitmask[i])
                comb.insert(set[i]);
        }
        result.push_back(comb);
    } while (std::next_permutation(bitmask.begin(), bitmask.end()));

    return result;
}

std::map<int, std::vector<std::set<int>>> generate_all_subsets_by_size(
    const std::vector<int> &set,
    int max_size
)
{
    std::map<int, std::vector<std::set<int>>> all_combinations;

    for (int k = 0; k <= max_size; ++k)
    {
        all_combinations[k] = generate_combinations(set, k);
    }

    return all_combinations;
}

std::vector<std::vector<std::set<int>>> prepare_all_candidates(
    const std::vector<int> &agent_ids,
    const std::vector<int> &firm_capacities)
{
    int max_capacity = *std::max_element(firm_capacities.begin(), firm_capacities.end());
    auto subset_map = generate_all_subsets_by_size(agent_ids, max_capacity);

    std::vector<std::vector<std::set<int>>> all_candidates;
    for (int cap : firm_capacities)
    {
        std::vector<std::set<int>> merged;
        for (int k = 0; k <= cap; ++k)
        {
            const auto &subsets = subset_map.at(k);
            merged.insert(merged.end(), subsets.begin(), subsets.end());
        }
        all_candidates.push_back(merged);
    }
    return all_candidates;
};

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
    const std::set<int> &agent_col_match,
    const std::vector<int> &agent_pref,
    const std::vector<int> &agent_col_pref)
{   
    int score = 0;
    for (int agent_col : agent_col_match) score += agent_col_pref[agent_col];
    score += agent_pref[firm];
    return score;
}

std::map<int,int> agent_scores_mp(
    const int &firm,
    const std::set<int> &firm_matching,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    std::map<int,int> score_mp;
    for (int agent : firm_matching) {
        int agent_score = compute_agent_score(firm, firm_matching, agent_prefs[agent], agent_col_prefs[agent]);
        score_mp[agent] = agent_score;
    }

    return score_mp;
}