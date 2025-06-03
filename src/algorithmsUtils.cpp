#include "algorithmsUtils.hpp"

std::vector<std::pair<int, std::vector<int>>> make_matching_base(const int& n_firms) {
    // 各企業ごとにmatchingの元を作成
    std::vector<std::pair<int, std::vector<int>>> matching(n_firms);
    for (int i = 0; i < n_firms; ++i)
    {
        matching[i].first = i;
    }
    return matching;
}