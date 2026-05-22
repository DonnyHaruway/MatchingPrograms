#include "Utils.hpp"
#include "CommonTypes.hpp"

using namespace MatchingTypes;

std::vector<int> generate_random_ranked(int size, int min, std::mt19937& rng, std::string type, int who)
{
    std::vector<int> order(size);

    if (type == "opponent")
    {
        std::iota(order.begin(), order.end(), min);
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
            if (i == who) {
                continue;
            }
            order[i] = others[idx++];
        }
    }
    else
    {
        throw std::invalid_argument("Unknown type: " + type);
    }

    return order;
}

std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng, std::string type, int who)
{
    if (min_val > max_val)
    {
        throw std::invalid_argument("min_val must not be greater than max_val");
    }

    std::uniform_int_distribution<> dist(min_val, max_val); // 閉区間[min_val, max_val]から乱数を生成
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

std::vector<int> generate_random_super_increasing(int size, std::mt19937& rng)
{
    if (size > 30) {
        throw std::overflow_error("Size is too large for int super-increasing sequence (max approx 30 elements for base 2).");
    }

    std::vector<int> order(size); 

    long long current_val = 1; 

    for (int i = 0; i < size; ++i) {
        order[i] = static_cast<int>(current_val);
        current_val *= 2; 
    }

    std::shuffle(order.begin(), order.end(), rng);

    return order;
}

std::vector<int> generate_random_binary(int size, std::mt19937& rng, std::string type, int who)
{
    std::vector<int> v(size);

    if (type == "opponent")
    {
        std::uniform_int_distribution<> dist(0, 1); // 0か1を生成
        for (int &x : v)
            x = dist(rng) == 1 ? -1e9 : 0;
    }
    else if (type == "col")
    {
        v[who] = 0;
        std::uniform_int_distribution<> dist(0, 1); // 0か1を生成
        for (int i = 0; i < size; ++i)
        {
            if (i != who)
            {
                v[i] = dist(rng) == 1 ? -1e9 : 0;
            }
        }
    }
    else
    {
        throw std::invalid_argument("Unknown type: " + type);
    }
    return v;
}

static void combinations_helper(
    const std::vector<int> &set, int k, int start,
    std::vector<int> &current,
    std::vector<std::set<int>> &result
)
{
    if (k == 0)
    {
        result.emplace_back(current.begin(), current.end());
        return;
    }
    for (int i = start; i <= static_cast<int>(set.size()) - k; ++i)
    {
        current.push_back(set[i]);
        combinations_helper(set, k - 1, i + 1, current, result);
        current.pop_back();
    }
}

std::vector<std::set<int>> generate_combinations(const std::vector<int> &set, int k)
{
    std::vector<std::set<int>> result;
    std::vector<int> current;
    current.reserve(k);
    combinations_helper(set, k, 0, current, result);
    return result;
}

std::map<int, std::vector<std::set<int>>> generate_all_subsets_by_size(
    const std::vector<int> &set,
    int max_size
)
{
    std::map<int, std::vector<std::set<int>>> all_combinations;
    if (set.size() < max_size) max_size = set.size();
    for (int k = 0; k <= max_size; ++k)
    {
        all_combinations[k] = generate_combinations( set, k);
    }

    return all_combinations;
}

std::vector<std::vector<std::set<int>>> prepare_all_candidates(
    const std::vector<int> &agent_ids,
    const std::vector<int> &firm_capacities
)
{
    int max_capacity = *std::max_element(firm_capacities.begin(), firm_capacities.end());
    auto subset_map = generate_all_subsets_by_size(agent_ids, max_capacity);

    std::vector<std::vector<std::set<int>>> candidate_map;
    for (int cap : firm_capacities)
    {
        std::vector<std::set<int>> merged;
        for (int k = 0; k <= cap; ++k)
        {
            const auto &subsets = subset_map.at(k);
            merged.insert(merged.end(), subsets.begin(), subsets.end());
        }
        candidate_map.push_back(merged);
    }
    return candidate_map;
};

int compute_firm_score(
    const std::set<int> &firm_match,
    const std::vector<int> &firm_pref
)
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
    const std::vector<int> &agent_col_pref
)
{   
    if (firm == UNMATCHED) return 0;
    int score = 0;
    for (int agent_col : agent_col_match) {
        score += agent_col_pref[agent_col];
    }
    score += agent_pref[firm];
    return score;
}