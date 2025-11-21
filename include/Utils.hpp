#pragma once

#include <vector>
#include <random>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream>


/// @brief 順位付けされたランダムな整数ベクトルを生成する
/// @param size ベクトルのサイズ
/// @param min ベクトルの要素の最小値
/// @param rng 乱数生成器
/// @param type 選好の種類 ("opponent"または"col")
/// @param who 選好の所有者 (typeが"col"の場合に使用)
/// @return ランダムに順位付けされた整数ベクトル
std::vector<int> generate_random_ranked(int size, int min, std::mt19937& rng, std::string type="opponent", int who=-1);

/// @brief ランダムな整数ベクトルを生成する
/// @param size ベクトルのサイズ
/// @param min_val ベクトルの要素の最小値
/// @param max_val ベクトルの要素の最大値
/// @param rng 乱数生成器
/// @param type 選好の種類 ("opponent"または"col")
/// @param who 選好の所有者 (typeが"col"の場合に使用)
/// @return [min_val, max_val]の範囲でランダムに生成された整数ベクトル
std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937 &rng, std::string type="opponent", int who=-1);

/// @brief 集合の指定サイズの組み合わせを生成する
/// @param set 元の整数集合
/// @param k 組み合わせのサイズ
/// @return すべての組み合わせ
std::vector<std::set<int>> generate_combinations(const std::vector<int> &set, int k);

/// @brief 指定サイズまでの部分集合を生成する
/// @param set 元の整数集合
/// @param max_size 作成する部分集合の最大サイズ
/// @return 部分集合の要素数をキーとし、そのサイズの部分集合を値とするmap
std::map<int, std::vector<std::set<int>>> generate_all_subsets_by_size(
    const std::vector<int>& set,
    int max_size
);

/**
 * 各企業に対して、エージェントの割り当て候補グループを列挙する。
 *
 * @param agent_ids         割り当て可能なすべてのエージェントのIDリスト（0から始まる連番などを想定）。
 * @param firm_capacities   各企業が受け入れ可能なエージェント数のリスト。i番目の企業の上限は firm_capacities[i]。
 *
 * @return 各企業ごとに割り当て可能なエージェントの部分集合（候補グループ）のリスト。
 *         candidate_map[i]はi番目の企業に対する割り当て候補のエージェントの部分集合のリストを表す。  
 *
 */
std::vector<std::vector<std::set<int>>> prepare_all_candidates(
    const std::vector<int>& agent_ids,
    const std::vector<int>& firm_capacities
);

int compute_firm_score(
    const std::set<int>& firm_match,
    const std::vector<int>& firm_pref
);

/// @brief agent_colの自分自身のスコアは0前提で計算
/// @param firm 
/// @param firm_match 
/// @param agent_pref 
/// @param agent_col_pref 
/// @return 
int compute_agent_score(
    const int &firm,
    const std::set<int> &agent_col_match,
    const std::vector<int> &agent_pref,
    const std::vector<int> &agent_col_pref
);


/// @brief 全ての個人の現状のマッチに対する計算をして、mapで返す
/// @param firm 
/// @param firm_matching 
/// @param agent_prefs 
/// @param agent_col_prefs 
/// @return (agent, score)のmap
std::map<int,int> agent_scores_mp(
    const int &firm,
    const std::set<int> &firm_matching,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
);