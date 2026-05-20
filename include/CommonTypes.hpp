#pragma once
#include <set>
#include <utility>

// 頻出の型に対してエイリアスを作成
namespace MatchingTypes {
    // (企業, set<個人>)のpair
    using FirmMatching = std::pair<int, std::set<int>>;
}