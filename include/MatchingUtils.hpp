#pragma once
#include "Matching.hpp"
#include <vector>
#include <set>

/// @brief 再帰的に全てのマッチングを生成する
/// @param firm_idx 現在割り当てを行っている企業のインデックス
/// @param all_candidates 各企業に対する候補のエージェントグループ
/// @param current_matching 現在構築中のマッチング
/// @param used_agents 既にどこかの企業に割り当てられたエージェントの集合
/// @param result 有効なマッチングの結果を格納するvector
void generate_matchings_recursive(
    int firm_idx,
    const int &n_agents,
    const std::vector<std::vector<std::set<int>>> &all_candidates,
    std::vector<std::set<int>> &current_matching,
    std::set<int> &used_agents,
    std::vector<Matching> &result);