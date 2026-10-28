#pragma once

#include "rda_batch.hpp"
#include <limits>

namespace rda_exhaustive {

using rda_batch::Problem;
using rda_batch::Summary;
using universal_replacement_experiment::Rankings;
using Count = std::uint64_t;

// Public orders use 1-based counterpart labels and 0 for the outside option.
// Keep the order BELOW the outside option too: these are different strict profiles.
struct StrictOrder {
    std::vector<int> order;
    std::vector<int> scores;
};

inline std::vector<StrictOrder> strict_orders(int alternatives)
{
    std::vector<int> order(alternatives + 1);
    std::iota(order.begin(), order.end(), 0);
    std::vector<StrictOrder> result;
    do {
        const int outside_rank = static_cast<int>(std::find(order.begin(), order.end(), 0) - order.begin());
        std::vector<int> scores(alternatives);
        for (int rank = 0; rank <= alternatives; ++rank) {
            if (order[rank] != 0) scores[order[rank] - 1] = outside_rank - rank;
        }
        result.push_back({order, std::move(scores)});
    } while (std::next_permutation(order.begin(), order.end()));
    return result;
}

class Domain {
public:
    const int workers;
    const std::vector<int> capacities;
    const std::vector<StrictOrder> worker_orders;
    const std::vector<StrictOrder> firm_orders;
    const unsigned pair_count;
    const Count masks;
    const Count total;

    explicit Domain(int n)
        : workers(checked_workers(n)), capacities{2, n == 3 ? 1 : 2},
          worker_orders(strict_orders(2)), firm_orders(strict_orders(n)),
          pair_count(n * (n - 1) / 2), masks(Count{1} << pair_count),
          total(profile_count(n, worker_orders.size(), firm_orders.size()) * masks) {}

    Problem problem(Count id) const
    {
        if (id >= total) throw std::out_of_range("instance ID is outside the domain");
        const Count mask = id % masks;
        id /= masks;
        Problem result;
        result.capacities = capacities;
        result.firm_rankings.resize(2);
        result.firm_prefs.resize(2);
        for (int firm = 1; firm >= 0; --firm) {
            const auto &preference = firm_orders[id % firm_orders.size()];
            id /= firm_orders.size();
            result.firm_rankings[firm] = preference.order;
            result.firm_prefs[firm] = preference.scores;
        }
        result.worker_rankings.resize(workers);
        result.agent_prefs.resize(workers);
        for (int worker = workers - 1; worker >= 0; --worker) {
            const auto &preference = worker_orders[id % worker_orders.size()];
            id /= worker_orders.size();
            result.worker_rankings[worker] = preference.order;
            result.agent_prefs[worker] = preference.scores;
        }
        result.colleague_prefs.assign(workers, std::vector<int>(workers, 0));
        unsigned bit = 0;
        for (int left = 0; left < workers; ++left) {
            for (int right = left + 1; right < workers; ++right, ++bit) {
                if ((mask & (Count{1} << bit)) == 0) continue;
                result.forbidden_pairs.emplace_back(left + 1, right + 1);
                result.colleague_prefs[left][right] = -MatchingTypes::INF;
                result.colleague_prefs[right][left] = -MatchingTypes::INF;
            }
        }
        return result;
    }

private:
    static int checked_workers(int n)
    {
        if (n != 3 && n != 4) throw std::invalid_argument("--workers must be 3 or 4");
        return n;
    }

    static Count profile_count(int n, Count worker_orders, Count firm_orders)
    {
        Count profiles = firm_orders * firm_orders;
        for (int i = 0; i < n; ++i) profiles *= worker_orders;
        return profiles;
    }
};

struct Range {
    Count begin, end;
    Count size() const { return end - begin; }
};

inline Range range(const Domain &domain, Count start, std::optional<Count> count)
{
    if (start >= domain.total) throw std::invalid_argument("--start-id is outside the domain");
    const Count length = count.value_or(domain.total - start);
    if (length == 0 || length > domain.total - start) {
        throw std::invalid_argument("--count must be positive and fit within the domain");
    }
    return {start, start + length};
}

struct Totals {
    Count inputs = 0, all_stable = 0, mixed = 0, none_stable = 0;
    Count nodes = 0, branches = 0, leaves = 0, stable_leaves = 0;
    Count distinct_matchings = 0, distinct_stable_matchings = 0;
    Count max_nodes = 0, max_deletions = 0;

    void add(const Summary &summary)
    {
        ++inputs;
        const auto category = summary.category();
        if (category == "all_stable") ++all_stable;
        else if (category == "mixed") ++mixed;
        else ++none_stable;
        nodes += summary.nodes;
        branches += summary.branches;
        leaves += summary.leaves;
        stable_leaves += summary.stable_leaves;
        distinct_matchings += summary.distinct_matchings;
        distinct_stable_matchings += summary.distinct_stable_matchings;
        max_nodes = std::max(max_nodes, summary.nodes);
        max_deletions = std::max(max_deletions, static_cast<Count>(summary.max_deletions));
    }
};

} // namespace rda_exhaustive
