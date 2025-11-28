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
        if (firm == -1) continue;
        for (int other_agent : firm_matchs[firm])
        {
            if (other_agent != i && other_agent != -1)
            {
                agent_col_matchs[i].insert(other_agent);
            }
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
        for (int agent : agents) {
            agent_matchs[agent] = firm_id;
            firm_agents.insert(agent);
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
    if (scores_computed) return;
    agent_scores.resize(n_agent, 0);
    for (int agent = 0; agent < n_agent; agent++)
    {
        agent_scores[agent] = compute_agent_score(
            agent_matchs[agent],
            agent_col_matchs[agent],
            agent_prefs[agent],
            agent_col_prefs[agent]
        );
    }

    firm_scores.resize(n_firm, 0);
    for (int firm = 0; firm < n_firm; firm++)
    {
        firm_scores[firm] = compute_firm_score(firm_matchs[firm], firm_prefs[firm]);
        
    }
    scores_computed = true;
}

bool Matching::is_individually_rational(
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    this->compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    for (int agent=0; agent<n_agent; agent++) {
        if (agent_scores[agent] < 0) return false;
    }
    for (int firm=0; firm<n_firm; firm++) {
        if (firm_scores[firm] < 0) return false;
    }
    return true;
}

std::vector<std::pair<int,int>> Matching::blocking_pairs(
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    const std::vector<int> &firm_capacities
)
{
    this->compute_scores(agent_prefs, firm_prefs, agent_col_prefs);
    std::vector<std::pair<int,int>> blocking_pairs_list;
    for (int firm=0; firm<n_firm; firm++) {
        for (int agent=0; agent<n_agent; agent++) {

            std::vector<int> set_firm_match(firm_matchs[firm].begin(), firm_matchs[firm].end());
            std::map<int, std::vector<std::set<int>>> subset_map = generate_all_subsets_by_size(set_firm_match, firm_capacities[firm]);
            for (auto [size, subsets] : subset_map) {
                if (size+1 > firm_capacities[firm]) break;
                for (auto subset : subsets) {
                    if (subset.count(agent)) continue;
                    std::set<int> set_tmp = subset;
                    set_tmp.insert(agent);

                    int firm_score_after = compute_firm_score(set_tmp, firm_prefs[firm]);
                    if (firm_scores[firm] >= firm_score_after) continue;

                    int agent_score_after = compute_agent_score(firm, set_tmp, agent_prefs[agent], agent_col_prefs[agent]);
                    if (agent_scores[agent] >= agent_score_after) continue;

                    bool col_blocking_flag = true;
                    for (int agent_col : set_tmp) {
                        if (agent_col == agent) continue;
                        int col_score_after = compute_agent_score(firm, set_tmp, agent_prefs[agent_col], agent_col_prefs[agent_col]);
                        if (agent_scores[agent_col] > col_score_after) {
                            col_blocking_flag=false;
                            break;
                        }
                    }
                    if (!col_blocking_flag) continue;
                    blocking_pairs_list.emplace_back(agent, firm);
                }
            }
        }
    }
    return blocking_pairs_list;
}

bool Matching::is_stable(
        const std::vector<std::vector<int>> &agent_prefs,
        const std::vector<std::vector<int>> &firm_prefs,
        const std::vector<std::vector<int>> &agent_col_prefs,
        const std::vector<int> &firm_capacities
)
{   
    if (!is_individually_rational(agent_prefs, firm_prefs, agent_col_prefs)) return false;
    auto blocking_pairs_list = blocking_pairs(agent_prefs, firm_prefs, agent_col_prefs, firm_capacities);
    if (!blocking_pairs_list.empty()) return false;
    return true;
}

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
        if (firm_matchs[i].size() == 0)
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
        if (agent_col_matchs[i].size() == 0)
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