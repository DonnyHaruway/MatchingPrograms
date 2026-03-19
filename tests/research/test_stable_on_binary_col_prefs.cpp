#include <gtest/gtest.h>
#include "MatchingSystem.hpp"
#include "../algorithms/algorithms_test_helpers.hpp"
#include <algorithm>
#include <numeric>
#include <vector>
#include <chrono>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;

// =====================================================================
// ヘルパー関数
// =====================================================================

// {1, 2, ..., n} の全順列を返す
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

// n×n の二値同僚選好行列の全パターンを返す
// 対角成分は 0 固定, 非対角成分は 0 or BINARY_PREF_NEG_INF
static vector<vector<vector<int>>> all_binary_col_prefs(int n)
{
    // 非対角成分のインデックスリストを作成
    vector<pair<int,int>> offdiag;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) offdiag.emplace_back(i, j);

    int m = offdiag.size(); // n*(n-1)
    int total = 1 << m;

    vector<vector<vector<int>>> result;
    result.reserve(total);

    for (int mask = 0; mask < total; mask++) {
        vector<vector<int>> mat(n, vector<int>(n, 0));
        for (int k = 0; k < m; k++) {
            if (mask & (1 << k)) {
                mat[offdiag[k].first][offdiag[k].second] = BINARY_PREF_NEG_INF;
            }
        }
        result.push_back(move(mat));
    }
    return result;
}

// Cartesian product: perms を agents 個分組み合わせた全パターンを返す
// 結果は vector<vector<vector<int>>>（各要素が n_agents 行の選好行列）
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

// =====================================================================
// 全列挙チェック本体
// =====================================================================

static void run_exhaustive_check(int n_agents, int n_firms, const vector<int> &capacities)
{
    auto agent_perms  = all_permutations(n_firms);   // 各agentのfirm選好の全順列
    auto firm_perms   = all_permutations(n_agents);  // 各firmのagent選好の全順列
    auto col_pref_all = all_binary_col_prefs(n_agents);

    auto all_agent_prefs = cartesian_product(agent_perms, n_agents);
    auto all_firm_prefs  = cartesian_product(firm_perms,  n_firms);

    long long total = (long long)all_agent_prefs.size()
                    * all_firm_prefs.size()
                    * col_pref_all.size();
    long long cnt_no_weakly_stable = 0;
    int n_ap = (int)all_agent_prefs.size();

    #pragma omp parallel for reduction(+:cnt_no_weakly_stable) schedule(dynamic)
    for (int ai = 0; ai < n_ap; ai++) {
        const auto &agent_prefs = all_agent_prefs[ai];
        for (const auto &firm_prefs : all_firm_prefs) {
            for (const auto &col_prefs : col_pref_all) {
                MatchingSystem ms(n_agents, n_firms);
                ms.set_agent_prefs(agent_prefs);
                ms.set_firm_prefs(firm_prefs);
                ms.set_agent_col_prefs(col_prefs);
                ms.set_firm_capacities(capacities);

                if (!stable_matching_exists(ms, capacities)) {
                    cnt_no_weakly_stable++;
                    #pragma omp critical
                    {
                        cout << "=== weakly stable matching does NOT exist ===" << endl;
                        ms.print_prefs();
                        cout << endl;
                    }
                }
            }
        }
    }

    cout << "no stable: " << cnt_no_weakly_stable << " / " << total << endl;
}

// =====================================================================
// TEST ケース
// =====================================================================

TEST(Research, WeaklyStable)
{
    auto start = chrono::steady_clock::now();
    run_exhaustive_check(3, 2, {2, 1});
    auto end = chrono::steady_clock::now();
    double elapsed = chrono::duration<double>(end - start).count();
    cout << "elapsed: " << elapsed << " s" << endl;
}
