#pragma once

#include "Matching.hpp"
#include "algorithmsUtils.hpp"
#include <vector>
#include <utility>
#include <queue>
#include <functional>
#include <algorithm>
#include <random>
#include <set>
#include <map>
#include <optional>

// Optional decision control for exhaustive exploration of the modified/iterated DAs.
enum class DAChoiceKind { pair_order, removal_endpoint };

struct DAConflict {
    int firm;
    int left;
    int right;
};

struct DARandomChoice {
    DAChoiceKind kind;
    int round; // One-based proposal round (modified DA) or complete DA run (iterated DA).
    std::size_t option_count;
    // pair_order: remaining pairs (iterated DA chooses just one); removal_endpoint: the current pair.
    std::vector<DAConflict> conflicts;
    std::vector<int> agent_match; // Snapshot before this decision, zero-based IDs.
};

// Return an index in [0, option_count). Endpoints use 0=left, 1=right.
// Empty selector retains the original shuffle and random endpoint selection.
using DARandomChoiceSelector = std::function<std::size_t(const DARandomChoice &)>;

/// @brief agent をランダムな優先順で処理する dictator アルゴリズム。
/// @param seed 省略時は std::random_device から取得する (再現性が必要なら明示すること)
Matching doctor_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed = std::nullopt
);

Matching firm_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
);

Matching simple_match_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
);

Matching doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
);

/// @brief 企業が提案する DA アルゴリズム。同僚選好は考慮しない。
///        スコアは大きいほど好ましく、負のスコアは受け入れ不可。
///        企業の同順位は agent ID 順、個人の同順位は現在の企業を維持する。
Matching firm_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
);

/// @brief DAを最後まで実行し、同じ企業の拒否ペアがなくなるまで最初から再実行する。
///        拒否ペア (どちらかの方向が -INF) を全企業から1組だけ一様ランダムに選び、
///        片方を等確率でその企業の選好から削除 (-INF) する。削除をメモリ上で累積保持する。
///        再実行時に全マッチ・提案履歴を初期化する。入力の選好は変更しない。
///        doctor/firm proposing の各通常DAの受容・同順位の規則に従う。
/// @param seed 省略時は std::random_device。指定時は同じ入力・実行環境で再現可能。
/// @param choice_selector 全探索用。複数ペアからの1組の選択と削除対象を制御する。
Matching iterated_doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed = std::nullopt,
    const DARandomChoiceSelector &choice_selector = {}
);

/// @brief iterated_doctor_proposing_DA_algorithm の企業提案版。同じ削除・再実行規則を使う。
Matching iterated_firm_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed = std::nullopt,
    const DARandomChoiceSelector &choice_selector = {}
);

/// @brief 全企業の提案ラウンド後に、同僚の拒否ペアをランダムな順で解消する企業提案 DA。
///        各ラウンド開始時に空きのある企業が1人ずつ同時に提案する。
///        拒否ペア (-INF がどちらかの方向にあるペア) の片方を等確率で削除する。
///        削除はその企業に対して永久。他の仮マッチ・提案履歴は保持する。
///        企業の同順位は個人 ID 順、個人の同順位は現在の企業を維持する。
///        新規の同順位提案は企業 ID 順。入力の選好は変更しない。
/// @param seed 省略時は std::random_device。指定時は同じ入力・実行環境で再現可能。
/// @param choice_selector 全探索用。指定時はペアの処理順と削除対象の両方を制御する。
Matching modified_firm_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed = std::nullopt,
    const DARandomChoiceSelector &choice_selector = {}
);

/// @brief 全個人の提案ラウンド後に、同僚の拒否ペアをランダムな順で解消する個人提案 DA。
///        各ラウンド開始時に未マッチの個人が、未提案の企業1社に提案する。
///        企業は既存の仮マッチと新規提案から定員まで選ぶ。同順位は既存を優先し、個人 ID 順。
///        個人の同順位の企業は企業 ID 順。双方の負のスコアは受け入れ不可、0 は受容。
///        拒否ペアを全件シャッフルし、未解決のペアの片方を等確率で削除する。
///        削除はその企業に対して永久。他の仮マッチ・提案履歴を保持し、入力は変更しない。
/// @param seed 省略時は std::random_device。指定時は同じ入力・実行環境で再現可能。
/// @param choice_selector 全探索用。指定時はペアの処理順と削除対象の両方を制御する。
Matching modified_worker_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed = std::nullopt,
    const DARandomChoiceSelector &choice_selector = {}
);
