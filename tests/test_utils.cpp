#include <gtest/gtest.h>
#include "Utils.hpp"

using namespace std;

TEST(UtilTest, GenerateCombinations_Test) {
    const vector<int> elems = {0, 1, 2, 3};
    const int k = 2;
    const int expected_size = 6;
    auto result = generate_combinations(elems, k);
    EXPECT_EQ(result.size(), expected_size);

    set<vector<int>> st = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };
    for (auto cmb : result) {
        if (!st.count(cmb)) {
            cout << "Not found " << endl;
        }
    }
}

TEST(UtilTest, GenerateAllSubsetsBySize) {
    const vector<int> elems = {0, 1, 2, 3};
    const int k = 2;

    const int expected_size = 11;
    auto result = generate_all_subsets_by_size(elems, k);
    EXPECT_EQ(result.size(), expected_size);

    set<vector<int>> st = {
        {0, 1},
        {0, 2},
        {0, 3},
        {1, 2},
        {1, 3},
        {2, 3}
    };
}