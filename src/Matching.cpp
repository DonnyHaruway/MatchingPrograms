#include "Matching.hpp"

Matching::Matching(const std::vector<int> &agent_match,
                   const std::vector<std::vector<int>> &firm_match)
    : agent_match(agent_match), firm_match(firm_match) {}

Matching Matching::from_firm_assignment(
    const std::vector<std::vector<int>>& firm_match) 
{
    int max_agent_id = -1;
    for (const auto& agents : firm_match) {
        for (int agent : agents) {
            max_agent_id = std::max(max_agent_id, agent);
        }
    }

    std::vector<int> agent_match(max_agent_id + 1, -1);

    for (int firm_id = 0; firm_id < firm_match.size(); ++firm_id) {
        for (int agent : firm_match[firm_id]) {
            agent_match[agent] = firm_id;
        }
    }

    return Matching(agent_match, firm_match);
}

void Matching::compute_scores(
  const std::vector<std::vector<int>>& agent_prefs,
  const std::vector<std::vector<int>>& firm_prefs,
  const std::vector<std::vector<int>>& agent_col_prefs
) {
  // agent側の計算
  agent_scores.resize(agent_prefs.size(), 0);
  for (int agent=0; agent < agent_prefs.size(); agent++) {
    int matched_firm = agent_match[agent];
    // 未割り当ての場合
    if (matched_firm==-1) continue;
    agent_scores[agent] += agent_prefs[agent][matched_firm];
    for (int agent_col : firm_match[matched_firm]) {
      if (agent==agent_col) continue;
      agent_scores[agent] += agent_col_prefs[agent][agent_col];
    }
  }

  // firm側の計算
  firm_scores.resize(firm_prefs.size(), 0);
  for (int firm=0; firm < firm_prefs.size(); firm++) {
    for (int agent : firm_match[firm]) {
      firm_scores[firm] += firm_prefs[firm][agent];
    }
  }
  scores_computed = true;
}

// アクセッサ
const std::vector<int>& Matching::get_agent_match() const {
  return agent_match;
};
const std::vector<std::vector<int>>& Matching::get_firm_match() const {
  return firm_match;
};
const std::vector<std::vector<int>> Matching::get_agent_col_match() const {
  auto agent_match = this->get_agent_match();
  auto firm_match = this->get_firm_match();
  
  std::vector<std::vector<int>> agent_col_match(agent_match.size());

  for (int i=0; i<agent_match.size(); i++) {
    for (int agent : firm_match[agent_match[i]]) {
      if (agent==i) continue;
      agent_col_match[i].push_back(agent);
    }
  }
  return agent_col_match;
}

const std::vector<int>& Matching::get_agent_scores() const {
  if (!scores_computed) {
      throw std::runtime_error("Agent scores have not been computed yet.");
  }
  return agent_scores;
}
const std::vector<int>& Matching::get_firm_scores() const {
  if (!scores_computed) {
      throw std::runtime_error("Firm scores have not been computed yet.");
  }
  return firm_scores;
}