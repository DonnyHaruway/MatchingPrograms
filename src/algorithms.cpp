#include "algorithms.hpp"
#include <queue>
#include <algorithm>
#include <random>

std::pair<
    std::vector<std::pair<int, std::vector<int>>>,
    std::pair<std::vector<int>, std::vector<int>>>
run_dictator_like_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs)
{
    // step 1: agentをランダムに並べてqueueに格納
    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(agent_ids.begin(), agent_ids.end(), rng);

    std::queue<int> agent_queue;
    for (int id : agent_ids)
        agent_queue.push(id);

    // [Step2以降の準備]
    std::vector<std::pair<int, std::vector<int>>> matching(n_firms);
    for (int i = 0; i < n_firms; ++i)
    {
        matching[i].first = i;
    }

    std::vector<int> firm_scores(n_firms, 0);
    std::vector<int> agent_scores(n_agents, 0);

    while (!agent_queue.empty())
    {
        int agent = agent_queue.front();
        agent_queue.pop();

        bool matched = false;

        // 各 firm の現在のマッチングと比較して最良の firm を探す
        int best_firm = -1;
        int best_score = -1;

        for (int i = 0; i < n_firms; ++i)
        {
            const auto &assigned_agents = matching[i].second;

            // firm i が agent をどれだけ評価してるか
            if (i >= agent_prefs[agent].size())
                continue;
            int firm_score = agent_prefs[agent][i];
            if (firm_score <= 0)
                continue; // 評価が非正 → オファーしない

            int score_sum = firm_score;

            // マッチ済みの agent たちから見た「このagent」への評価値
            bool all_accept = true;
            for (int other_agent : assigned_agents)
            {
                if (other_agent >= agent_col_prefs.size() || agent >= agent_col_prefs[other_agent].size())
                {
                    all_accept = false;
                    break;
                }
                int col_score = agent_col_prefs[other_agent][agent];
                if (col_score < 0)
                {
                    all_accept = false;
                    break;
                }
                score_sum += col_score;
            }

            if (all_accept && score_sum > best_score)
            {
                best_firm = i;
                best_score = score_sum;
            }
        }

        // ベストな firm に受け入れてもらえるかどうか
        if (best_firm != -1 && (int)matching[best_firm].second.size() < firm_capacities[best_firm])
        {
            matching[best_firm].second.push_back(agent);
            firm_scores[best_firm] += agent_prefs[agent][best_firm]; // agent → firm への評価
            for (int other_agent : matching[best_firm].second)
            {
                if (other_agent != agent)
                {
                    agent_scores[other_agent] += agent_col_prefs[other_agent][agent];
                    agent_scores[agent] += agent_col_prefs[agent][other_agent];
                }
            }
            matched = true;
        }

        if (!matched)
        {
            agent_queue.push(agent); // 条件満たさず → 後回し
        }
    }
    return {matching, {firm_scores, agent_scores}};
}