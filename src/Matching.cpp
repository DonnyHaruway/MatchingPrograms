#include "Matching.hpp"

Matching::Matching(
    const std::vector<int> &agent_matchs,
    const std::vector<std::set<int>> &firm_matchs
)
    : agent_matchs(agent_matchs), firm_matchs(firm_matchs), n_agent(agent_matchs.size()), n_firm(firm_matchs.size())
{
    agent_col_matchs.resize(n_agent);

    for (int i = 0; i < n_agent; ++i)
    {
        int firm = agent_matchs[i];
        if (firm == -1)
        {
            agent_col_matchs[i].insert(-1); // マッチしていない
            continue;
        }

        for (int other_agent : firm_matchs[firm])
        {
            if (other_agent != i && other_agent != -1)
            {
                agent_col_matchs[i].insert(other_agent);
            }
        }

        // 同僚がいなかった場合
        if (agent_col_matchs[i].empty())
        {
            agent_col_matchs[i].insert(-1);
        }
    }
}

Matching Matching::from_firm_assignment(
    const std::vector<std::set<int>> &input_firm_match,
    const int &n_agents
)
{
    std::vector<int> agent_matchs(n_agents, -1);
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
                agent_matchs[agent] = firm_id;
                firm_agents.insert(agent);
            }
        }

        firm_match.push_back(firm_agents);
    }

    return Matching(agent_matchs, firm_match);
}

void Matching::compute_scores(
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    // agent側の計算
    agent_scores.resize(n_agent, 0);
    for (int agent = 0; agent < n_agent; agent++)
    {
        int matched_firm = agent_matchs[agent];
        // 未割り当ての場合
        if (matched_firm == -1)
            continue;
        agent_scores[agent] += agent_prefs[agent][matched_firm];
        for (int agent_col : firm_matchs[matched_firm])
        {
            if (agent == agent_col)
                continue;
            agent_scores[agent] += agent_col_prefs[agent][agent_col];
        }
    }

    // firm側の計算
    firm_scores.resize(n_firm, 0);
    for (int firm = 0; firm < n_firm; firm++)
    {
        for (int agent : firm_matchs[firm])
        {
            firm_scores[firm] += firm_prefs[firm][agent];
        }
    }
    scores_computed = true;
}

bool Matching::is_stable(
        const std::vector<std::vector<int>> &agent_prefs,
        const std::vector<std::vector<int>> &firm_prefs,
        const std::vector<std::vector<int>> &agent_col_prefs,
        const std::vector<int> &firm_capacities
    )
{   
    // (firm, agent)が逸脱ペアであるかどうかを確認する
    for (int firm=0; firm<n_firm; firm++) {
        for (int agent=0; agent<n_agent; agent++) {
            int firm_score_before = compute_firm_score(firm_matchs[firm], firm_prefs[firm]);
            std::map<int,int> agent_scores_before = agent_scores_mp(firm, firm_matchs[firm], agent_prefs, agent_col_prefs);
            std::vector<int> set_firm_match(firm_matchs[firm].begin(), firm_matchs[firm].end());
            std::map<int, std::vector<std::set<int>>> subset_map = generate_all_subsets_by_size(set_firm_match, firm_capacities[firm]);
            for (auto [size, subset] : subset_map) {
                if (size+1 > firm_capacities[firm]) continue;
                for (auto set : subset) {
                    if (set.count(agent)) continue;
                    std::set<int> set_tmp = set;
                    set_tmp.insert(agent);
                    int firm_score_after = compute_firm_score(set_tmp, firm_prefs[firm]);
                    if (firm_score_before >= firm_score_after) continue;
                    bool blocking_flag = true;
                    for (int agent_matched : set_tmp) {
                        int agent_score_after = compute_agent_score(firm, set_tmp, agent_prefs[agent_matched], agent_col_prefs[agent_matched]);
                        if (agent_scores_before[agent_matched] >= agent_score_after) {
                            blocking_flag=false;
                            break;
                        }
                    }
                    if (!blocking_flag) continue;
                    // もしblocking pairだったらfalseを返す
                    return false;
                }
            }
        }
    }
    return true;
}

// print関数
void Matching::print() const
{
    std::cout << "=== Matching Information ===\n";

    std::cout << "\n[Agent Match]\n";
    for (int i = 0; i < n_agent; ++i)
    {
        std::cout << "  Agent " << i << " → Firm ";
        if (agent_matchs[i] == -1)
            std::cout << "unmatched";
        else
            std::cout << agent_matchs[i];
        std::cout << "\n";
    }

    std::cout << "\n[Firm Match]\n";
    for (int i = 0; i < n_firm; ++i)
    {
        std::cout << "  Firm " << i << " ← ";
        if (firm_matchs[i].size() == 1 && firm_matchs[i] == std::set<int>{-1})
        {
            std::cout << "unmatched";
        }
        else
        {
            for (int a : firm_matchs[i])
                std::cout << a << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\n[Agent Colleagues]\n";
    for (int i = 0; i < n_agent; ++i)
    {
        std::cout << "  Agent " << i << " has colleagues: ";
        if (agent_col_matchs[i].size() == 1 && agent_col_matchs[i] == std::set<int>{-1})
        {
            std::cout << "none";
        }
        else
        {
            for (int c : agent_col_matchs[i])
                std::cout << c << " ";
        }
        std::cout << "\n";
    }

    if (scores_computed)
    {
        std::cout << "\n[Agent Scores]\n";
        for (int i = 0; i < n_agent; ++i)
        {
            std::cout << "  Agent " << i << ": " << agent_scores[i] << "\n";
        }

        std::cout << "\n[Firm Scores]\n";
        for (int i = 0; i < n_firm; ++i)
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
    // agent_matchsとfirm_matchsが両方とも等しければ、trueを返す
    return this->agent_matchs == other.agent_matchs &&
            this->firm_matchs == other.firm_matchs;
}

bool operator!=(const Matching &lhs, const Matching &rhs)
{
    return !(lhs == rhs);
}

// アクセッサ
const std::vector<int> &Matching::get_agent_matchs() const
{
    return agent_matchs;
};
const std::vector<std::set<int>> &Matching::get_firm_matchs() const
{
    return firm_matchs;
};
const std::vector<std::set<int>> &Matching::get_agent_col_matchs() const
{
    return agent_col_matchs;
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