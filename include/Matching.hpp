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

        /// @brief return matching is stable (satify there is no type1 blocking pair and type2' blocking pair) or not
        /// @param agent_prefs 
        /// @param firm_prefs 
        /// @param agent_col_prefs 
        /// @param firm_capacities 
        /// @return boolean
        bool is_stable(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs,
            const std::vector<int> &firm_capacities
        );

        /// @brief return weakly blocking pairs 
        /// @param agent_prefs 
        /// @param firm_prefs 
        /// @param agent_col_prefs 
        /// @param firm_capacities 
        /// @return std::vector<(agent, firm)>
        std::vector<std::pair<int,int>> weakly_blocking_pairs(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs,
            const std::vector<int> &firm_capacities
        );

        /// @brief return matching is weakly stable (satify there is no weakly blocking pair) or not
        /// @param agent_prefs 
        /// @param firm_prefs 
        /// @param agent_col_prefs 
        /// @param firm_capacities 
        /// @return boolean
        bool is_weakly_stable(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs,
            const std::vector<int> &firm_capacities
        );

        /// @brief Return (agent, firm) universal-replacement blocking pairs.
        /// The agent must strictly prefer its firm score to the current firm's (unmatched = 0).
        /// With a vacancy, adding the agent must strictly improve the firm's score
        /// (agent score > 0 in the firm's preferences) and leave no colleague rejection.
        /// At a full firm, the firm must prefer the agent to at least one incumbent,
        /// and replacing EVERY incumbent must leave no colleague rejection in either direction.
        /// Intended for binary colleague preferences; includes vacancy-addition blocking.
        std::vector<std::pair<int,int>> universal_replacement_blocking_pairs(
            const std::vector<std::vector<int>> &agent_prefs,
            const std::vector<std::vector<int>> &firm_prefs,
            const std::vector<std::vector<int>> &agent_col_prefs,
            const std::vector<int> &firm_capacities
        );

        /// @brief Feasible, individually rational, and without universal-replacement blocking pairs.
        bool is_universal_replacement_stable(
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
