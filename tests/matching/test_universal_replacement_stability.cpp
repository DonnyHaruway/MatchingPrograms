#include <gtest/gtest.h>
#include "Matching.hpp"
#include "CommonTypes.hpp"

using namespace std;
using namespace MatchingTypes;

class UniversalReplacementTest : public ::testing::Test {
protected:
    vector<vector<int>> agent_prefs = {{1}, {1}, {1}};
    vector<vector<int>> firm_prefs = {{5, 1, 3}};
    vector<vector<int>> col_prefs = vector<vector<int>>(3, vector<int>(3, 0));
    vector<int> capacities = {2};
    vector<set<int>> assignment = {{0, 1}};

    vector<pair<int, int>> pairs() {
        auto matching = Matching::from_firm_assignment(assignment, 3);
        return matching.universal_replacement_blocking_pairs(
            agent_prefs, firm_prefs, col_prefs, capacities);
    }

    bool stable() {
        auto matching = Matching::from_firm_assignment(assignment, 3);
        return matching.is_universal_replacement_stable(
            agent_prefs, firm_prefs, col_prefs, capacities);
    }
};

TEST_F(UniversalReplacementTest, BlocksWhenOnlyOneIncumbentIsLessPreferred) {
    const vector<pair<int, int>> expected = {{2, 0}};
    EXPECT_EQ(pairs(), expected);
    EXPECT_FALSE(stable());
}

TEST_F(UniversalReplacementTest, ChecksReplacementOfMorePreferredIncumbentToo) {
    // Replacing agent 1 (the only less-preferred incumbent) is feasible.
    // Replacing agent 0 leaves an incompatible pair, so universal blocking fails.
    col_prefs[1][2] = col_prefs[2][1] = -INF;
    EXPECT_TRUE(pairs().empty());
    EXPECT_TRUE(stable());
}

TEST_F(UniversalReplacementTest, FeasibilityRequiresAcceptanceInBothDirections) {
    col_prefs[2][0] = -INF;
    EXPECT_TRUE(pairs().empty());
    col_prefs[2][0] = 0;
    col_prefs[0][2] = -INF;
    EXPECT_TRUE(pairs().empty());
}

TEST_F(UniversalReplacementTest, VacancyDoesNotBlockUnderTheScreenshotDefinition) {
    capacities[0] = 3;
    EXPECT_TRUE(pairs().empty());
    EXPECT_TRUE(stable());
}

TEST_F(UniversalReplacementTest, ZeroCapacityDoesNotBlockVacuously) {
    assignment[0].clear();
    capacities[0] = 0;
    EXPECT_TRUE(pairs().empty());
    EXPECT_TRUE(stable());
}

TEST_F(UniversalReplacementTest, FirmIndifferenceDoesNotBlock) {
    firm_prefs[0][2] = 1;
    EXPECT_TRUE(pairs().empty());
    EXPECT_TRUE(stable());
}

TEST_F(UniversalReplacementTest, WorkerMustStrictlyPreferFirmToBeingUnmatched) {
    agent_prefs[2][0] = 0;
    EXPECT_TRUE(pairs().empty());
    agent_prefs[2][0] = -1;
    EXPECT_TRUE(pairs().empty());
}

TEST_F(UniversalReplacementTest, MatchedWorkerMustStrictlyPreferDestinationFirm) {
    assignment.push_back({2});
    capacities.push_back(1);
    for (auto &prefs : agent_prefs) prefs.push_back(1);
    firm_prefs.push_back({0, 0, 1});
    EXPECT_TRUE(pairs().empty());
    agent_prefs[2][0] = 2;
    const vector<pair<int, int>> expected = {{2, 0}};
    EXPECT_EQ(pairs(), expected);
}

TEST_F(UniversalReplacementTest, SingleSeatCanReplaceAnIncompatibleIncumbent) {
    assignment[0] = {0};
    capacities[0] = 1;
    firm_prefs[0] = {1, 0, 3};
    agent_prefs[1][0] = 0;
    col_prefs[0][2] = col_prefs[2][0] = -INF;
    const vector<pair<int, int>> expected = {{2, 0}};
    EXPECT_EQ(pairs(), expected);
}

TEST_F(UniversalReplacementTest, ExistingPairsAreNotBlockingPairs) {
    agent_prefs[2][0] = 0;
    EXPECT_TRUE(pairs().empty());
}

TEST_F(UniversalReplacementTest, InfeasibleCurrentMatchingCannotBeStable) {
    // A large firm preference cannot cancel a hard colleague rejection.
    agent_prefs[0][0] = agent_prefs[1][0] = INF + 1;
    col_prefs[0][1] = col_prefs[1][0] = -INF;
    agent_prefs[2][0] = 0;
    EXPECT_TRUE(pairs().empty());
    EXPECT_FALSE(stable());
}

TEST_F(UniversalReplacementTest, OverCapacityMatchingCannotBeStable) {
    capacities[0] = 1;
    EXPECT_TRUE(pairs().empty());
    EXPECT_FALSE(stable());
}

TEST_F(UniversalReplacementTest, IndividualRationalityIsRequiredForWorkersAndFirms) {
    agent_prefs[2][0] = 0;
    agent_prefs[0][0] = -1;
    EXPECT_FALSE(stable());
    agent_prefs[0][0] = 1;
    firm_prefs[0] = {-2, 1, 0};
    EXPECT_FALSE(stable());
}
