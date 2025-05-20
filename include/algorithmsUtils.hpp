#pragma

#include <vector>
#include <utility>

/// @brief make base of matchings from number of firms
/// @param n_firms 
/// @return return matching base (vector of pairs of firm id and vector of agent ids)
std::vector<std::pair<int, std::vector<int>>> make_matching_base(const int& n_firms);