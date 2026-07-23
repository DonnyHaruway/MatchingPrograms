#pragma once
#include <set>
#include <string>
#include <utility>

// 頻出の型に対してエイリアスを作成
namespace MatchingTypes {
    // (企業, set<個人>)のpair
    using FirmMatching = std::pair<int, std::set<int>>;

    constexpr int UNMATCHED = -1;

    /// 「絶対に選ばれない」ことを表す十分大きな値。
    /// スコアの総和を取ってもintがオーバーフローしない大きさにしてある。
    constexpr int INF = 100000; // 1e5

    /// 選好を生成するときの既定のスコア範囲
    constexpr int DEFAULT_SCORE_MIN = 1;  // 全員にマッチするインセンティブがある状況を既定とする
    constexpr int DEFAULT_SCORE_MAX = 10; // 仮の値

    /// @brief 選好の向き。
    /// opp : 相手方への選好 (個人->企業, 企業->個人)
    /// col : 同僚への選好 (個人->個人)。自分自身へのスコアは常に0になる。
    enum class Side {
        opp,
        col
    };

    /// @brief 選好の生成方式。
    /// none            : 選好を作らない (すべて0)
    /// ranked          : 同順位なしの順位付け
    /// numeric         : 指定範囲の一様乱数スコア (同順位あり)
    /// binary          : 受容(0) か 拒否(-INF) の二値。colのみ対応。
    /// super_increasing: 超増加列。部分集合ごとの合計スコアが一意に定まる。
    enum class PrefKind {
        none,
        ranked,
        numeric,
        binary,
        super_increasing
    };

    inline std::string to_string(Side side)
    {
        switch (side) {
            case Side::opp: return "opp";
            case Side::col: return "col";
        }
        return "unknown";
    }

    inline std::string to_string(PrefKind kind)
    {
        switch (kind) {
            case PrefKind::none:             return "none";
            case PrefKind::ranked:           return "ranked";
            case PrefKind::numeric:          return "numeric";
            case PrefKind::binary:           return "binary";
            case PrefKind::super_increasing: return "super_increasing";
        }
        return "unknown";
    }
}
