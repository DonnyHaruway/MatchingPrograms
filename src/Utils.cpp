#include "Utils.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

std::vector<int> generate_random_ranked(int size, std::mt19937& rng) {
    std::vector<int> order(size);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    return order;
}

std::vector<int> generate_random_number(int size, int min_val, int max_val, std::mt19937& rng) {
    if (min_val > max_val) {
        throw std::invalid_argument("min_val must not be greater than max_val");
    }

    std::uniform_int_distribution<> dist(min_val, max_val);
    std::vector<int> v(size);
    for (int& x : v) x = dist(rng);
    return v;
}