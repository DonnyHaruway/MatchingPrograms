#pragma

#include "Matching.hpp"
#include <vector>
#include <random>
#include <map>
#include <set>

/// @brief 相手に対するランク順をランダムに生成する
/// @param size 相手の人数
/// @param rng 乱数生成器
/// @return ランキング（整数のvector）
std::vector<int> generate_random_ranked(int size, std::mt19937& rng);

/// @brief 相手に対する評価値をランダムに生成する
/// @param size 相手の人数
/// @param min_val 評価値の最小値
/// @param max_val 評価値の最大値
/// @param rng 乱数生成器
/// @return 評価値（整数のvector）
std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng);

/// @brief 集合の指定サイズの組み合わせを生成する
/// @param set 元の整数集合
/// @param k 組み合わせのサイズ
/// @return すべての組み合わせ
std::vector<std::vector<int>> generate_combinations(const std::vector<int>& set, int k);

/// @brief 指定サイズまでの部分集合を生成する
/// @param set 元の整数集合
/// @param max_size 作成する部分集合の最大サイズ
/// @return 部分集合の要素数をキーとし、そのサイズの部分集合を値とするmap
std::map<int, std::vector<std::vector<int>>> generate_all_subsets_by_size(
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
 *         外側の vector は企業ごと、中間の vector は各企業に対する候補グループのリスト、
 *         内側の vector はその候補グループに含まれるエージェントIDの集合を表す。
 *
 *         例：return[i][j][k] は、i 番目の企業にとって j 番目の候補グループの中の
 *         k 番目のエージェントIDを表す。
 */
std::vector<std::vector<std::vector<int>>> prepare_all_candidates(
    const std::vector<int>& agent_ids,
    const std::vector<int>& firm_capacities
);

/// @brief 再帰的に全てのマッチングを生成する
/// @param firm_idx 現在割り当てを行っている企業のインデックス
/// @param all_candidates 各企業に対する候補のエージェントグループ
/// @param current_matching 現在構築中のマッチング
/// @param used_agents 既にどこかの企業に割り当てられたエージェントの集合
/// @param result 有効なマッチングの結果を格納するvector
void generate_matchings_recursive(
    int firm_idx,
    const std::vector<std::vector<std::vector<int>>>& all_candidates,
    std::vector<std::vector<int>>& current_matching,
    std::set<int>& used_agents,
    std::vector<Matching>& result
);