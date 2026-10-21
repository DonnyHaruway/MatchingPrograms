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

TEST_F(UniversalReplacementTest, VacancyAdditionBlocks) {
    capacities[0] = 3;
    EXPECT_EQ(pairs(), (vector<pair<int, int>>{{2, 0}}));
    EXPECT_FALSE(stable());
}

TEST_F(UniversalReplacementTest, VacancyDoesNotRequireAnInferiorIncumbent) {
    capacities[0] = 3;
    firm_prefs[0][2] = 1;
    EXPECT_EQ(pairs(), (vector<pair<int, int>>{{2, 0}}));
    EXPECT_FALSE(stable());
}

TEST_F(UniversalReplacementTest, VacancyRequiresPositiveFirmPreference) {
    capacities[0] = 3;
    for (int score : {0, -1, -INF}) {
        firm_prefs[0][2] = score;
        EXPECT_TRUE(pairs().empty());
        EXPECT_TRUE(stable());
    }
}

TEST_F(UniversalReplacementTest, VacancyRequiresColleagueAcceptanceInBothDirections) {
    capacities[0] = 3;
    for (bool incoming_rejects : {false, true}) {
        col_prefs = vector<vector<int>>(3, vector<int>(3, 0));
        col_prefs[incoming_rejects ? 2 : 0][incoming_rejects ? 0 : 2] = -INF;
        EXPECT_TRUE(pairs().empty());
        EXPECT_TRUE(stable());
    }
}

TEST_F(UniversalReplacementTest, VacancyWorkerMustStrictlyPreferDestination) {
    assignment.push_back({2});
    capacities = {3, 1};
    for (auto &prefs : agent_prefs) prefs.push_back(1);
    firm_prefs.push_back({1, 1, 1});
    EXPECT_TRUE(pairs().empty());
    agent_prefs[2][0] = 2;
    EXPECT_EQ(pairs(), (vector<pair<int, int>>{{2, 0}}));
    EXPECT_FALSE(stable());
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

TEST(VacancyStability, EmptyMatchingWithAnAcceptablePairIsUnstableUnderAllPredicates) {
    const vector<vector<int>> workers = {{1}}, firms = {{1}}, colleagues = {{0}};
    const vector<int> capacities = {1};
    auto matching = Matching::from_firm_assignment({{}}, 1);
    EXPECT_EQ(matching.blocking_pairs(workers, firms, colleagues, capacities), (vector<pair<int, int>>{{0, 0}}));
    EXPECT_EQ(matching.weakly_blocking_pairs(workers, firms, colleagues, capacities), (vector<pair<int, int>>{{0, 0}}));
    EXPECT_EQ(matching.universal_replacement_blocking_pairs(workers, firms, colleagues, capacities), (vector<pair<int, int>>{{0, 0}}));
    EXPECT_FALSE(matching.is_stable(workers, firms, colleagues, capacities));
    EXPECT_FALSE(matching.is_weakly_stable(workers, firms, colleagues, capacities));
    EXPECT_FALSE(matching.is_universal_replacement_stable(workers, firms, colleagues, capacities));
}

TEST(VacancyStability, VacancyAdditionWithColleagueConsentIsRecognizedByAllPredicates) {
    const vector<vector<int>> workers = {{1}, {1}}, firms = {{2, 1}}, colleagues = {{0, 0}, {0, 0}};
    const vector<int> capacities = {2};
    auto matching = Matching::from_firm_assignment({{0}}, 2);
    EXPECT_FALSE(matching.is_stable(workers, firms, colleagues, capacities));
    EXPECT_FALSE(matching.is_weakly_stable(workers, firms, colleagues, capacities));
    EXPECT_FALSE(matching.is_universal_replacement_stable(workers, firms, colleagues, capacities));
}
