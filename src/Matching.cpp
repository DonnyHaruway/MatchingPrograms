#include "Matching.hpp"

Matching::Matching(const std::vector<int> &agent_match,
                   const std::vector<std::set<int>> &firm_match)
    : agent_match(agent_match), firm_match(firm_match)
{
    int n_agents = agent_match.size();
    agent_col_match.resize(n_agents);

    for (int i = 0; i < n_agents; ++i)
    {
        int firm = agent_match[i];
        if (firm == -1)
        {
            agent_col_match[i].insert(-1); // マッチしていない
            continue;
        }

        for (int other_agent : firm_match[firm])
        {
            if (other_agent != i && other_agent != -1)
            {
                agent_col_match[i].insert(other_agent);
            }
        }

        // 同僚がいなかった場合
        if (agent_col_match[i].empty())
        {
            agent_col_match[i].insert(-1);
        }
    }
}

Matching Matching::from_firm_assignment(
    const std::vector<std::set<int>> &input_firm_match,
    const int &n_agents)
{
    std::vector<int> agent_match(n_agents, -1);
    std::vector<std::set<int>> firm_match;

    for (int firm_id = 0; firm_id < input_firm_match.size(); ++firm_id)
    {
        const auto &agents = input_firm_match[firm_id];
        std::set<int> firm_agents;

        if (agents.empty())
        {
            firm_agents.insert(-1); // 誰ともマッチしていない
        }
        else
        {
            for (int agent : agents)
            {
                agent_match[agent] = firm_id;
                firm_agents.insert(agent);
            }
        }

        firm_match.push_back(firm_agents);
    }

    return Matching(agent_match, firm_match);
}

void Matching::compute_scores(
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs)
{
    // agent側の計算
    agent_scores.resize(agent_prefs.size(), 0);
    for (int agent = 0; agent < agent_prefs.size(); agent++)
    {
        int matched_firm = agent_match[agent];
        // 未割り当ての場合
        if (matched_firm == -1)
            continue;
        agent_scores[agent] += agent_prefs[agent][matched_firm];
        for (int agent_col : firm_match[matched_firm])
        {
            if (agent == agent_col)
                continue;
            agent_scores[agent] += agent_col_prefs[agent][agent_col];
        }
    }

    // firm側の計算
    firm_scores.resize(firm_prefs.size(), 0);
    for (int firm = 0; firm < firm_prefs.size(); firm++)
    {
        for (int agent : firm_match[firm])
        {
            firm_scores[firm] += firm_prefs[firm][agent];
        }
    }
    scores_computed = true;
}

// print関数
void Matching::print() const
{
    std::cout << "=== Matching Information ===\n";

    std::cout << "\n[Agent Match]\n";
    for (int i = 0; i < agent_match.size(); ++i)
    {
        std::cout << "  Agent " << i << " → Firm ";
        if (agent_match[i] == -1)
            std::cout << "unmatched";
        else
            std::cout << agent_match[i];
        std::cout << "\n";
    }

    std::cout << "\n[Firm Match]\n";
    for (int i = 0; i < firm_match.size(); ++i)
    {
        std::cout << "  Firm " << i << " ← ";
        if (firm_match[i].size() == 1 && firm_match[i] == std::set<int>{-1})
        {
            std::cout << "unmatched";
        }
        else
        {
            for (int a : firm_match[i])
                std::cout << a << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\n[Agent Colleagues]\n";
    for (int i = 0; i < agent_col_match.size(); ++i)
    {
        std::cout << "  Agent " << i << " has colleagues: ";
        if (agent_col_match[i].size() == 1 && agent_col_match[i] == std::set<int>{-1})
        {
            std::cout << "none";
        }
        else
        {
            for (int c : agent_col_match[i])
                std::cout << c << " ";
        }
        std::cout << "\n";
    }

    if (scores_computed)
    {
        std::cout << "\n[Agent Scores]\n";
        for (int i = 0; i < agent_scores.size(); ++i)
        {
            std::cout << "  Agent " << i << ": " << agent_scores[i] << "\n";
        }

        std::cout << "\n[Firm Scores]\n";
        for (int i = 0; i < firm_scores.size(); ++i)
        {
            std::cout << "  Firm " << i << ": " << firm_scores[i] << "\n";
        }
    }
    else
    {
        std::cout << "\n[Scores not computed yet]\n";
    }

    std::cout << "=============================\n";
}

bool Matching::operator==(const Matching &other) const
{
    // agent_matchとfirm_matchが両方とも等しければ、trueを返す
    return this->agent_match == other.agent_match &&
            this->firm_match == other.firm_match;
}

bool operator!=(const Matching &lhs, const Matching &rhs)
{
    return !(lhs == rhs);
}

// アクセッサ
const std::vector<int> &Matching::get_agent_match() const
{
    return agent_match;
};
const std::vector<std::set<int>> &Matching::get_firm_match() const
{
    return firm_match;
};
const std::vector<std::set<int>> Matching::get_agent_col_match() const
{
    return agent_col_match;
}

const std::vector<int> &Matching::get_agent_scores() const
{
    if (!scores_computed)
    {
        throw std::runtime_error("Agent scores have not been computed yet.");
    }
    return agent_scores;
}
const std::vector<int> &Matching::get_firm_scores() const
{
    if (!scores_computed)
    {
        throw std::runtime_error("Firm scores have not been computed yet.");
    }
    return firm_scores;
}