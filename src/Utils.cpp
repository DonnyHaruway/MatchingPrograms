#include "Utils.hpp"
#include "CommonTypes.hpp"

using namespace MatchingTypes;

namespace
{
    /// @brief type と who の組み合わせを検証する
    /// @return Side::col なら所有者のindex、Side::opp なら UNMATCHED (使わない)
    int resolve_who(Side type, int size, std::optional<int> who)
    {
        if (size <= 0)
        {
            throw std::invalid_argument("size must be positive");
        }

        if (type == Side::opp)
        {
            if (who.has_value())
            {
                throw std::invalid_argument("who must not be specified when type is opp");
            }
            return UNMATCHED;
        }

        if (!who.has_value())
        {
            throw std::invalid_argument("who is required when type is col");
        }
        if (*who < 0 || *who >= size)
        {
            throw std::out_of_range("who is out of range: " + std::to_string(*who));
        }
        return *who;
    }
}

std::vector<int> generate_random_ranked(
    Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who,
    std::optional<int> min
)
{
    const int owner = resolve_who(type, size, who);
    const int min_val = min.value_or(DEFAULT_SCORE_MIN);

    std::vector<int> order(size);

    if (type == Side::opp)
    {
        std::iota(order.begin(), order.end(), min_val);
        std::shuffle(order.begin(), order.end(), rng);
        return order;
    }

    // 自分自身は0固定なので、他人にはmin以上の整数から0を除いてsize-1個を割り当てる
    std::vector<int> others;
    others.reserve(size - 1);
    for (int v = min_val; static_cast<int>(others.size()) < size - 1; ++v)
    {
        if (v == 0) continue;
        others.push_back(v);
    }
    std::shuffle(others.begin(), others.end(), rng);

    order[owner] = 0;
    int idx = 0;
    for (int i = 0; i < size; ++i)
    {
        if (i == owner) continue;
        order[i] = others[idx++];
    }

    return order;
}

std::vector<int> generate_random_numeric(
    Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who,
    std::optional<int> min_val,
    std::optional<int> max_val
)
{
    const int owner = resolve_who(type, size, who);

    if (min_val.has_value() != max_val.has_value())
    {
        throw std::invalid_argument("min_val and max_val must be specified together");
    }
    const int lo = min_val.value_or(DEFAULT_SCORE_MIN);
    const int hi = max_val.value_or(DEFAULT_SCORE_MAX);
    if (lo > hi)
    {
        throw std::invalid_argument("min_val must not be greater than max_val");
    }

    std::uniform_int_distribution<> dist(lo, hi); // 閉区間[lo, hi]から乱数を生成
    std::vector<int> v(size);

    for (int i = 0; i < size; ++i)
    {
        // 自分自身は0になる
        v[i] = (i == owner) ? 0 : dist(rng);
    }
    return v;
}

std::vector<int> generate_random_super_increasing(
    Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who
)
{
    const int owner = resolve_who(type, size, who);

    // 自分自身の0を除いた要素数ぶんだけ超増加列を作る
    const int n_values = (type == Side::col) ? size - 1 : size;
    if (n_values > 30) {
        throw std::overflow_error("Size is too large for int super-increasing sequence (max approx 30 elements for base 2).");
    }

    std::vector<int> values(n_values);
    long long current_val = 1;
    for (int i = 0; i < n_values; ++i) {
        values[i] = static_cast<int>(current_val);
        current_val *= 2;
    }
    std::shuffle(values.begin(), values.end(), rng);

    if (type == Side::opp) return values;

    std::vector<int> order(size);
    order[owner] = 0;
    int idx = 0;
    for (int i = 0; i < size; ++i)
    {
        if (i == owner) continue;
        order[i] = values[idx++];
    }
    return order;
}

std::vector<int> generate_random_binary(
    Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who,
    std::optional<int> penalty
)
{
    if (type != Side::col)
    {
        throw std::invalid_argument("binary preferences are only defined for type col");
    }
    const int owner = resolve_who(type, size, who);
    const int reject_score = penalty.value_or(-INF);

    std::vector<int> v(size);
    std::uniform_int_distribution<> dist(0, 1); // 0か1を生成
    for (int i = 0; i < size; ++i)
    {
        // 自分自身は0になる
        v[i] = (i == owner) ? 0 : (dist(rng) == 1 ? reject_score : 0);
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