#pragma once

#include "CommonTypes.hpp"

#include <vector>
#include <random>
#include <map>
#include <optional>
#include <set>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream>

/*
 * generate_random_* 系の共通仕様
 *
 * 必須引数は (type, size, rng) の3つのみで、残りはすべて省略可能。
 *
 *   type == Side::opp : 相手方への選好。whoを渡すと例外。
 *   type == Side::col : 同僚への選好。whoは必須で、who番目の要素は常に0になる。
 *
 * whoが範囲外(0 <= who < size を満たさない)の場合も例外を投げる。
 */

/// @brief 順位付けされたランダムな整数ベクトルを生成する
/// @param type 選好の向き (Side::opp または Side::col)
/// @param size ベクトルのサイズ
/// @param rng 乱数生成器
/// @param who 選好の所有者 (Side::colのとき必須)
/// @param min 要素の最小値 (省略時 DEFAULT_SCORE_MIN)
///            Side::colのときは自分自身の0と衝突しないよう、min以上の整数から0を除いて割り当てる
/// @return ランダムに順位付けされた整数ベクトル
std::vector<int> generate_random_ranked(
    MatchingTypes::Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who = std::nullopt,
    std::optional<int> min = std::nullopt
);

/// @brief 指定範囲の一様乱数からなる整数ベクトルを生成する
/// @param type 選好の向き (Side::opp または Side::col)
/// @param size ベクトルのサイズ
/// @param rng 乱数生成器
/// @param who 選好の所有者 (Side::colのとき必須)
/// @param min_val 要素の最小値 (省略時 DEFAULT_SCORE_MIN)
/// @param max_val 要素の最大値 (省略時 DEFAULT_SCORE_MAX)
/// @note min_val と max_val は「両方指定」か「両方省略」のみ許可する
/// @return [min_val, max_val]の範囲でランダムに生成された整数ベクトル
std::vector<int> generate_random_numeric(
    MatchingTypes::Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who = std::nullopt,
    std::optional<int> min_val = std::nullopt,
    std::optional<int> max_val = std::nullopt
);

/// @brief ランダムな超増加列を生成する
/// @param type 選好の向き (Side::opp または Side::col)
/// @param size ベクトルのサイズ
/// @param rng 乱数生成器
/// @param who 選好の所有者 (Side::colのとき必須)
/// @note Side::colのときは自分自身が0になり、残りのsize-1個に 1,2,4,...,2^(size-2) を割り当てる
/// @return ランダムに生成された超増加列
std::vector<int> generate_random_super_increasing(
    MatchingTypes::Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who = std::nullopt
);

/// @brief ランダムな二値ベクトルを生成する (同僚への選好専用)
/// @param type 選好の向き。Side::col のみ対応 (Side::oppは例外)
/// @param size ベクトルのサイズ
/// @param rng 乱数生成器
/// @param who 選好の所有者 (必須)
/// @param penalty 拒否する同僚に与えるスコア (省略時 -INF)
/// @return ランダムに生成された二値ベクトル (0 または penalty)
std::vector<int> generate_random_binary(
    MatchingTypes::Side type,
    int size,
    std::mt19937 &rng,
    std::optional<int> who = std::nullopt,
    std::optional<int> penalty = std::nullopt
);

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

/// @brief Calculates the score of an agent based on their matched firm and matched colleagues.
/// @param firm 
/// @param agent_col_match 
/// @param agent_pref 
/// @param agent_col_pref 
/// @return 
int compute_agent_score(
    const int &firm,
    const std::set<int> &agent_col_match,
    const std::vector<int> &agent_pref,
    const std::vector<int> &agent_col_pref
);