#pragma once

#include "Utils.hpp"
#include <vector>
#include <iostream>
#include <set>

class Matching
{
    private:
        int n_agent;
        int n_firm;
        std::vector<int> agent_matchs;
        std::vector<std::set<int>> firm_matchs;
        std::vector<std::set<int>> agent_col_matchs;
        std::vector<int> agent_scores;
        std::vector<int> firm_scores;
        bool scores_computed = false;

    public:
        Matching() = default;

        Matching(
            const std::vector<int> &agent_match, 
            const std::vector<std::set<int>> &firm_match
        );

        static Matching from_firm_assignment(
            const std::vector<std::set<int>> &firmMatch,
            const int &n_agent_in
        );

        void compute_scores(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs
        );

        bool is_individually_rational(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs
        );

        std::vector<std::pair<int,int>> blocking_pairs(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs,
            const std::vector<int> &firm_capacities
        );

        bool is_stable(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs,
            const std::vector<int> &firm_capacities
        );

        void print() const;

        bool operator==(const Matching &other) const;
        const std::vector<int> &get_agent_matchs() const;
        const std::vector<std::set<int>> &get_firm_matchs() const;
        const std::vector<std::set<int>> &get_agent_col_matchs() const;
        const std::vector<int> &get_agent_scores() const;
        const std::vector<int> &get_firm_scores() const;
};

bool operator!=(const Matching &lhs, const Matching &rhs);