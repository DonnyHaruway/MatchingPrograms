#pragma once

#include "algorithms.hpp"
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace universal_replacement_experiment {

struct ChoiceDecision {
    DARandomChoice choice;
    std::size_t selected;
};

struct ChoiceSearchStats {
    std::uint64_t replay_calls = 0;
    std::uint64_t completed_paths = 0;
    std::size_t max_decisions = 0;
    std::size_t max_simultaneous_conflicts = 0;
};

// Run the actual algorithm from the start with each decision prefix. At the
// next unspecified decision, recurse into ALL alternatives. Never sample seeds.
// For modified DAs, enumerate pair orders including orders with skipped pairs.
// For iterated DAs, enumerate the single selected pair before each restart.
template<class Runner, class Visitor>
ChoiceSearchStats enumerate_choice_paths(Runner runner, Visitor visitor)
{
    struct NeedChoice { std::size_t options; };
    ChoiceSearchStats stats;
    std::function<void(std::vector<std::size_t>)> visit;
    visit = [&](std::vector<std::size_t> prefix) {
        ++stats.replay_calls;
        std::size_t position = 0;
        std::vector<ChoiceDecision> decisions;
        DARandomChoiceSelector selector = [&](const DARandomChoice &choice) {
            if (choice.option_count < 2) throw std::logic_error("decision must have at least two options");
            stats.max_simultaneous_conflicts = std::max(stats.max_simultaneous_conflicts, choice.conflicts.size());
            if (position == prefix.size()) throw NeedChoice{choice.option_count};
            const auto selected = prefix[position++];
            if (selected >= choice.option_count) throw std::logic_error("decision prefix out of range");
            decisions.push_back({choice, selected});
            return selected;
        };
        try {
            Matching result = runner(selector);
            if (position != prefix.size()) throw std::logic_error("unused decision prefix");
            ++stats.completed_paths;
            stats.max_decisions = std::max(stats.max_decisions, decisions.size());
            visitor(stats.completed_paths, result, decisions);
        } catch (const NeedChoice &choice) {
            prefix.push_back(0);
            for (std::size_t index = 0; index < choice.options; ++index) {
                prefix.back() = index;
                visit(prefix);
            }
        }
    };
    visit({});
    return stats;
}

} // namespace universal_replacement_experiment
