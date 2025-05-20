#include "Matching.hpp"

Matching::Matching(const std::vector<int> &agentMatch,
                   const std::vector<std::vector<int>> &firmMatch)
    : agent_to_firm(agentMatch),
      firm_to_agents(firmMatch) {}