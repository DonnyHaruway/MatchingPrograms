#include "MatchingSystem.hpp"
#include "../tests/algorithms/algorithms_test_helpers.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <set>
#include <numeric>
#include <vector>

using namespace std;

namespace {

struct SearchResult {
    bool found_non_weakly_stable = false;
    long long tested_instances = 0;
    long long skipped_by_agent_symmetry = 0;
    vector<vector<int>> witness_agent_prefs;
    vector<vector<int>> witness_firm_prefs;
    vector<vector<int>> witness_col_prefs;
};

static vector<vector<int>> all_permutations(int n)
{
    vector<int> base(n);
    iota(base.begin(), base.end(), 1);
    vector<vector<int>> result;
    do {
        result.push_back(base);
    } while (next_permutation(base.begin(), base.end()));
    return result;
}

static vector<vector<int>> all_index_permutations(int n)
{
    vector<int> base(n);
    iota(base.begin(), base.end(), 0);
    vector<vector<int>> result;
    do {
        result.push_back(base);
    } while (next_permutation(base.begin(), base.end()));
    return result;
}

static void cartesian_product_rec(
    const vector<vector<int>> &perms,
    int depth,
    int total_depth,
    vector<vector<int>> &current,
    vector<vector<vector<int>>> &out)
{
    if (depth == total_depth) {
        out.push_back(current);
        return;
    }
    for (const auto &p : perms) {
        current.push_back(p);
        cartesian_product_rec(perms, depth + 1, total_depth, current, out);
        current.pop_back();
    }
}

static vector<vector<vector<int>>> cartesian_product(const vector<vector<int>> &perms, int times)
{
    vector<vector<vector<int>>> out;
    vector<vector<int>> current;
    cartesian_product_rec(perms, 0, times, current, out);
    return out;
}

static vector<vector<int>> build_binary_col_prefs_from_mask(int n_agents, uint64_t mask)
{
    vector<vector<int>> col_prefs(n_agents, vector<int>(n_agents, 0));
    int bit_index = 0;
    for (int i = 0; i < n_agents; i++) {
        for (int j = 0; j < n_agents; j++) {
            if (i == j) {
                continue;
            }
            if ((mask >> bit_index) & 1ULL) {
                col_prefs[i][j] = BINARY_PREF_NEG_INF;
            }
            bit_index++;
        }
    }
    return col_prefs;
}

static int compare_transformed_to_identity(
    const vector<vector<int>> &agent_prefs,
    const vector<vector<int>> &firm_prefs,
    const vector<vector<int>> &col_prefs,
    const vector<int> &perm,
    const vector<int> &inv)
{
    const int n_agents = static_cast<int>(agent_prefs.size());
    const int n_firms = static_cast<int>(firm_prefs.size());

    for (int i = 0; i < n_agents; i++) {
        for (int j = 0; j < n_firms; j++) {
            const int transformed = agent_prefs[perm[i]][j];
            const int identity = agent_prefs[i][j];
            if (transformed < identity) return -1;
            if (transformed > identity) return 1;
        }
    }

    for (int f = 0; f < n_firms; f++) {
        for (int k = 0; k < n_agents; k++) {
            const int old_agent_idx = firm_prefs[f][k] - 1;
            const int transformed = inv[old_agent_idx] + 1;
            const int identity = firm_prefs[f][k];
            if (transformed < identity) return -1;
            if (transformed > identity) return 1;
        }
    }

    for (int i = 0; i < n_agents; i++) {
        for (int j = 0; j < n_agents; j++) {
            const int transformed = col_prefs[perm[i]][perm[j]];
            const int identity = col_prefs[i][j];
            if (transformed < identity) return -1;
            if (transformed > identity) return 1;
        }
    }

    return 0;
}

static bool is_canonical_under_agent_relabel(
    const vector<vector<int>> &agent_prefs,
    const vector<vector<int>> &firm_prefs,
    const vector<vector<int>> &col_prefs,
    const vector<vector<int>> &perms,
    const vector<vector<int>> &inv_perms)
{
    for (size_t p = 1; p < perms.size(); p++) {
        if (compare_transformed_to_identity(agent_prefs, firm_prefs, col_prefs, perms[p], inv_perms[p]) < 0) {
            return false;
        }
    }
    return true;
}

static SearchResult run_exhaustive_check(int n_agents, int n_firms, const vector<int> &capacities)
{
    SearchResult result;

    const auto agent_perms = all_permutations(n_firms);
    const auto firm_perms = all_permutations(n_agents);

    const auto all_agent_prefs = cartesian_product(agent_perms, n_agents);
    const auto all_firm_prefs = cartesian_product(firm_perms, n_firms);
    const auto relabel_perms = all_index_permutations(n_agents);
    vector<vector<int>> inv_relabel_perms;
    inv_relabel_perms.reserve(relabel_perms.size());
    for (const auto &perm : relabel_perms) {
        vector<int> inv(perm.size());
        for (int i = 0; i < static_cast<int>(perm.size()); i++) {
            inv[perm[i]] = i;
        }
        inv_relabel_perms.push_back(move(inv));
    }

    const int offdiag = n_agents * (n_agents - 1);
    const uint64_t col_pref_total = 1ULL << offdiag;

    const long long n_ap = static_cast<long long>(all_agent_prefs.size());
    const long long n_fp = static_cast<long long>(all_firm_prefs.size());
    const long long pair_count = n_ap * n_fp;

    atomic<bool> found(false);
    long long tested_instances = 0;
    long long skipped_by_agent_symmetry = 0;

    #pragma omp parallel for schedule(dynamic) reduction(+:tested_instances,skipped_by_agent_symmetry)
    for (long long pair_idx = 0; pair_idx < pair_count; pair_idx++) {
        if (found.load(memory_order_relaxed)) {
            continue;
        }

        const long long ai = pair_idx / n_fp;
        const long long fi = pair_idx % n_fp;
        const auto &agent_prefs = all_agent_prefs[ai];
        const auto &firm_prefs = all_firm_prefs[fi];

        for (uint64_t mask = 0; mask < col_pref_total; mask++) {
            if (found.load(memory_order_relaxed)) {
                break;
            }

            const auto col_prefs = build_binary_col_prefs_from_mask(n_agents, mask);

            if (!is_canonical_under_agent_relabel(
                agent_prefs,
                firm_prefs,
                col_prefs,
                relabel_perms,
                inv_relabel_perms)) {
                skipped_by_agent_symmetry++;
                continue;
            }

            MatchingSystem ms(n_agents, n_firms);
            ms.set_agent_prefs(agent_prefs);
            ms.set_firm_prefs(firm_prefs);
            ms.set_agent_col_prefs(col_prefs);
            ms.set_firm_capacities(capacities);

            tested_instances++;

            if (!weakly_stable_matching_exists(ms, capacities)) {
                #pragma omp critical
                {
                    if (!found.load(memory_order_relaxed)) {
                        found.store(true, memory_order_relaxed);
                        result.found_non_weakly_stable = true;
                        result.witness_agent_prefs = agent_prefs;
                        result.witness_firm_prefs = firm_prefs;
                        result.witness_col_prefs = col_prefs;
                    }
                }
                break;
            }
        }
    }

    result.tested_instances = tested_instances;
    result.skipped_by_agent_symmetry = skipped_by_agent_symmetry;

    return result;
}

static vector<vector<int>> enumerate_capacities_two_firms_ordered(int n_agents)
{
    vector<vector<int>> capacities;
    for (int c0 = 1; c0 <= n_agents - 1; c0++) {
        capacities.push_back({c0, n_agents - c0});
    }
    return capacities;
}

static vector<vector<int>> compress_capacities_by_firm_swap(const vector<vector<int>> &ordered_caps)
{
    vector<vector<int>> compressed;
    set<pair<int, int>> seen;

    for (const auto &cap : ordered_caps) {
        const int a = min(cap[0], cap[1]);
        const int b = max(cap[0], cap[1]);
        if (seen.insert({a, b}).second) {
            compressed.push_back({a, b});
        }
    }
    return compressed;
}

static void print_matrix(const vector<vector<int>> &mat)
{
    for (const auto &row : mat) {
        for (size_t j = 0; j < row.size(); j++) {
            cout << row[j] << (j + 1 == row.size() ? '\n' : ' ');
        }
    }
}

} // namespace

int main()
{
    const int min_agents = 2;
    const int max_agents = 5;
    const int n_firms = 2;

    cout << "Exhaustive weak-stability search on binary colleague preferences" << endl;
    cout << "agents: " << min_agents << ".." << max_agents << ", firms: " << n_firms << endl;
    cout << "capacity rule: each firm >= 1 and sum(capacities)=n_agents" << endl;

    for (int n_agents = min_agents; n_agents <= max_agents; n_agents++) {
        const auto ordered_caps = enumerate_capacities_two_firms_ordered(n_agents);
        const auto capacities_list = compress_capacities_by_firm_swap(ordered_caps);
        cout << "\n=== n_agents=" << n_agents << ", n_firms=" << n_firms << " ===" << endl;
        cout << "capacity compression (ordered -> unique under firm swap): "
             << ordered_caps.size() << " -> " << capacities_list.size() << endl;

        for (const auto &capacities : capacities_list) {
            auto start = chrono::steady_clock::now();
            const auto result = run_exhaustive_check(n_agents, n_firms, capacities);
            auto end = chrono::steady_clock::now();
            const double elapsed = chrono::duration<double>(end - start).count();

            cout << "capacities: [" << capacities[0] << ", " << capacities[1] << "]" << endl;
            cout << "tested instances: " << result.tested_instances << endl;
              cout << "skipped by agent-label symmetry: " << result.skipped_by_agent_symmetry << endl;
            cout << "found non-weakly-stable instance: "
                 << (result.found_non_weakly_stable ? "YES" : "NO") << endl;
            cout << "elapsed: " << elapsed << " s" << endl;

            if (result.found_non_weakly_stable) {
                cout << "witness agent prefs:" << endl;
                print_matrix(result.witness_agent_prefs);
                cout << "witness firm prefs:" << endl;
                print_matrix(result.witness_firm_prefs);
                cout << "witness colleague prefs:" << endl;
                print_matrix(result.witness_col_prefs);
            }
        }
    }

    return 0;
}
