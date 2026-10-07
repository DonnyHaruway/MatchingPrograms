#include "algorithms.hpp"
#include "CommonTypes.hpp"
#include <iostream>
#include <iterator>
#include <tuple>
#include <stdexcept>

using namespace MatchingTypes;

namespace {
void resolve_colleague_conflicts(
    std::vector<std::set<int>> &firm_match,
    std::vector<int> &agent_match,
    std::vector<std::vector<bool>> &deleted,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::mt19937 &rng,
    int round,
    const DARandomChoiceSelector &choice_selector
)
{
    // Randomly permute all current rejection pairs across all firms.
    // Removing workers cannot introduce a new pair, so one pass is enough.
    std::vector<DAConflict> conflicts;
    for (int firm = 0; firm < static_cast<int>(firm_match.size()); ++firm) {
        const auto &agents = firm_match[firm];
        for (auto left = agents.begin(); left != agents.end(); ++left) {
            for (auto right = std::next(left); right != agents.end(); ++right) {
                if (agent_col_prefs[*left][*right] == -INF
                    || agent_col_prefs[*right][*left] == -INF) {
                    conflicts.push_back({firm, *left, *right});
                }
            }
        }
    }
    if (choice_selector) {
        std::vector<DAConflict> ordered;
        while (conflicts.size() > 1) {
            const auto index = choice_selector({DAChoiceKind::pair_order, round,
                                               conflicts.size(), conflicts, agent_match});
            if (index >= conflicts.size()) throw std::out_of_range("DA pair-order choice out of range");
            ordered.push_back(conflicts[index]);
            conflicts.erase(conflicts.begin() + index);
        }
        ordered.insert(ordered.end(), conflicts.begin(), conflicts.end());
        conflicts = std::move(ordered);
    } else {
        std::shuffle(conflicts.begin(), conflicts.end(), rng);
    }
    std::uniform_int_distribution<int> choose_endpoint(0, 1);
    for (const auto &[firm, left, right] : conflicts) {
        if (agent_match[left] != firm || agent_match[right] != firm) continue;
        const auto endpoint = choice_selector
            ? choice_selector({DAChoiceKind::removal_endpoint, round, 2,
                               {{firm, left, right}}, agent_match})
            : static_cast<std::size_t>(choose_endpoint(rng));
        if (endpoint >= 2) throw std::out_of_range("DA removal choice out of range");
        const int removed = endpoint == 0 ? left : right;
        deleted[firm][removed] = true;
        firm_match[firm].erase(removed);
        agent_match[removed] = UNMATCHED;
    }
}
}

Matching doctor_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed
)
{
    std::mt19937 rng(seed.has_value() ? *seed : std::random_device{}());

    std::vector<int> agent_ids(n_agents);
    std::iota(agent_ids.begin(), agent_ids.end(), 0);
    std::shuffle(agent_ids.begin(), agent_ids.end(), rng);
    std::queue<int> agent_queue;
    for (int id : agent_ids) {
        agent_queue.push(id);
    }

    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<std::set<FirmMatching>> unofferable_list(n_agents);

    while (!agent_queue.empty())
    {   

        int agent = agent_queue.front();
        agent_queue.pop();

        std::vector<FirmMatching> current_firm_match_subsets = create_current_firm_match_subsets(firm_match, firm_capacities);
        FirmMatching prefered_match = find_prefered_match(agent, agent_prefs[agent], agent_col_prefs[agent], current_firm_match_subsets, unofferable_list[agent]);
        if (prefered_match.first == -1) {
            continue;
        }
        unofferable_list[agent].emplace(prefered_match);
        if (is_firm_match_accept_propose(agent, prefered_match, firm_match[prefered_match.first], agent_prefs, firm_prefs, agent_col_prefs)) {
            std::set<int> firm_match_before = firm_match[prefered_match.first];
            std::set<int> firm_match_after = prefered_match.second;
            prefered_match.second.insert(agent);
            std::set<int> agent_deleted = find_agent_deleted(firm_match_before, firm_match_after);
                for (int a : agent_deleted) {
                    agent_queue.push(a);
                    unofferable_list[a].emplace(prefered_match.first, firm_match_after);
                }
            firm_match[prefered_match.first] = prefered_match.second;
        } else {
            agent_queue.push(agent);
        }
    }
    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching firm_dictator_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    std::vector<int> firm_ids(n_firms);
    std::iota(firm_ids.begin(), firm_ids.end(), 0);
    std::queue<int> firm_queue;
    for (int id : firm_ids) {
        firm_queue.push(id);
    }
    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<int> single_agents(n_agents);
    std::iota(single_agents.begin(), single_agents.end(), 0);
    while (!firm_queue.empty())
    {   
        int firm = firm_queue.front();
        firm_queue.pop();

        std::set<int> match = {};
        int current_score = 0;
        auto candidates_map = generate_all_subsets_by_size(single_agents, firm_capacities[firm]);
        for (int size = firm_capacities[firm]; size >= 0; size--) {
            for (auto candidate_set : candidates_map[size]) {
                // Check if all agents in candidate_set would accept the proposal
                if (!agents_accept_match(candidate_set, firm, agent_prefs, firm_prefs, agent_col_prefs)) continue;
                int firm_tmp_score = compute_firm_score(candidate_set, firm_prefs[firm]);
                if (firm_tmp_score < current_score) continue;
                match = candidate_set;
                current_score = firm_tmp_score;
            }
        }
        firm_match[firm] = match;
        for (int agent : match) {
            single_agents.erase(std::remove(single_agents.begin(), single_agents.end(), agent), single_agents.end());
        }

        std::cout << "Firm " << firm << " matched agents: ";
        for (int agent : match) {
            std::cout << agent << " ";
        }
        std::cout << std::endl;
    }
    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching simple_match_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs
)
{
    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<int> agent_match(n_agents, UNMATCHED);
    std::queue<int> q;
    for (int firm = 0; firm<n_firms; firm++) q.push(firm);
    while (q.size()) {
        int firm = q.front(); q.pop();
        // type1 blocking pairの導出
        int agent = find_best_type1_blocking_pair(firm, firm_match, agent_match, agent_prefs, firm_prefs, agent_col_prefs, firm_capacities);
        if (agent == -1) continue;

        // マッチングを確定
        int old_firm = agent_match[agent];
        if (old_firm != UNMATCHED) firm_match[old_firm].erase(agent);
        firm_match[firm].insert(agent);
        agent_match[agent] = firm;

        q.push(firm);
    }

    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching doctor_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
)
{
    // Min-heap: (preference_score, agent_id) → 先頭が最もスコアの低い（最悪の）エージェント
    using MinHeap = std::priority_queue<
        std::pair<int,int>,
        std::vector<std::pair<int,int>>,
        std::greater<std::pair<int,int>>
    >;
    std::vector<MinHeap> firm_heaps(n_firms);

    std::vector<std::vector<int>> agent_order(n_agents);
    #pragma omp parallel for
    for (int a = 0; a < n_agents; ++a) {
        std::vector<int> firms(n_firms);
        std::iota(firms.begin(), firms.end(), 0);
        std::sort(firms.begin(), firms.end(), [&](int f1, int f2){
            return agent_prefs[a][f1] > agent_prefs[a][f2];
        });
        agent_order[a] = firms;
    }
    std::vector<int> next_idx(n_agents, 0);
    std::queue<int> free_agents;
    for (int a = 0; a < n_agents; ++a) free_agents.push(a);

    while (!free_agents.empty()) {
        int agent = free_agents.front();
        free_agents.pop();
        if (next_idx[agent] >= n_firms) continue;
        int firm = agent_order[agent][next_idx[agent]];
        next_idx[agent]++;

        if (firm_prefs[firm][agent] < 0) {
            free_agents.push(agent);
            continue;
        }

        if ((int)firm_heaps[firm].size() < firm_capacities[firm]) {
            // firmに空きがあれば受け入れ
            firm_heaps[firm].push({firm_prefs[firm][agent], agent});
        } else {
            // min-heapの先頭 = 最もスコアの低いエージェント → O(1)
            auto [worst_score, worst_agent] = firm_heaps[firm].top();
            if (firm_prefs[firm][agent] > worst_score) {
                firm_heaps[firm].pop();  // worst_agentを除去 → O(log C)
                firm_heaps[firm].push({firm_prefs[firm][agent], agent});  // O(log C)
                free_agents.push(worst_agent);
            } else {
                free_agents.push(agent);
            }
        }
    }

    // heapをsetに変換してマッチングを構築
    std::vector<std::set<int>> firm_match(n_firms);
    #pragma omp parallel for
    for (int f = 0; f < n_firms; ++f) {
        while (!firm_heaps[f].empty()) {
            firm_match[f].insert(firm_heaps[f].top().second);
            firm_heaps[f].pop();
        }
    }
    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching firm_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs
)
{
    std::vector<std::vector<int>> firm_order(n_firms);
    #pragma omp parallel for
    for (int f = 0; f < n_firms; ++f) {
        std::vector<int> agents(n_agents);
        std::iota(agents.begin(), agents.end(), 0);
        std::sort(agents.begin(), agents.end(), [&](int a1, int a2){
            if (firm_prefs[f][a1] == firm_prefs[f][a2]) return a1 < a2;
            return firm_prefs[f][a1] > firm_prefs[f][a2];
        });
        firm_order[f] = agents;
    }

    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<int> agent_match(n_agents, UNMATCHED);
    std::vector<int> next_idx(n_firms, 0);
    std::queue<int> free_firms;
    for (int f = 0; f < n_firms; ++f) {
        if (firm_capacities[f] > 0) free_firms.push(f);
    }

    while (!free_firms.empty()) {
        int firm = free_firms.front();
        free_firms.pop();

        // 空きがある限り、まだ提案していないエージェントに選好順で提案
        while ((int)firm_match[firm].size() < firm_capacities[firm]
               && next_idx[firm] < n_agents) {
            int agent = firm_order[firm][next_idx[firm]];
            next_idx[firm]++;

            // これ以降も負のスコアなので、この企業の提案を終了
            if (firm_prefs[firm][agent] < 0) break;
            if (agent_prefs[agent][firm] < 0) continue;

            int old_firm = agent_match[agent];
            if (old_firm != UNMATCHED) {
                // 同順位の場合は現在の企業を維持
                if (agent_prefs[agent][firm] <= agent_prefs[agent][old_firm]) continue;

                // 満員だった企業に空きができた場合のみ再度キューに追加
                if ((int)firm_match[old_firm].size() == firm_capacities[old_firm]) {
                    free_firms.push(old_firm);
                }
                firm_match[old_firm].erase(agent);
            }

            firm_match[firm].insert(agent);
            agent_match[agent] = firm;
        }
    }

    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching modified_firm_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed,
    const DARandomChoiceSelector &choice_selector
)
{
    std::mt19937 rng(seed.has_value() ? *seed : std::random_device{}());
    std::vector<std::vector<int>> firm_order(n_firms);
    for (int firm = 0; firm < n_firms; ++firm) {
        auto &order = firm_order[firm];
        order.resize(n_agents);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int left, int right) {
            if (firm_prefs[firm][left] == firm_prefs[firm][right]) return left < right;
            return firm_prefs[firm][left] > firm_prefs[firm][right];
        });
    }

    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<int> agent_match(n_agents, UNMATCHED);
    std::vector<int> next_idx(n_firms, 0);
    std::vector<std::vector<bool>> deleted(n_firms, std::vector<bool>(n_agents, false));
    int round = 0;

    while (true) {
        // A: Decide all offers from the matching at the START of the round.
        // Resolve offers without altering vacancies until every firm has acted.
        auto next_agent_match = agent_match;
        bool made_proposal = false;
        for (int firm = 0; firm < n_firms; ++firm) {
            if (static_cast<int>(firm_match[firm].size()) >= firm_capacities[firm]) continue;
            while (next_idx[firm] < n_agents
                   && deleted[firm][firm_order[firm][next_idx[firm]]]) ++next_idx[firm];
            if (next_idx[firm] == n_agents) continue;

            const int candidate = firm_order[firm][next_idx[firm]++];
            if (firm_prefs[firm][candidate] < 0) {
                // The remaining candidates are also unacceptable to this firm.
                next_idx[firm] = n_agents;
                continue;
            }
            made_proposal = true;
            if (agent_prefs[candidate][firm] < 0) continue;

            const int current_offer = next_agent_match[candidate];
            if (current_offer == UNMATCHED
                || agent_prefs[candidate][firm] > agent_prefs[candidate][current_offer]) {
                next_agent_match[candidate] = firm;
            }
        }
        if (!made_proposal) break;

        agent_match = std::move(next_agent_match);
        for (auto &agents : firm_match) agents.clear();
        for (int agent = 0; agent < n_agents; ++agent) {
            if (agent_match[agent] != UNMATCHED) firm_match[agent_match[agent]].insert(agent);
        }

        // B: Resolve every rejection pair, retaining the proposal history.
        resolve_colleague_conflicts(firm_match, agent_match, deleted, agent_col_prefs, rng, ++round, choice_selector);
    }

    return Matching::from_firm_assignment(firm_match, n_agents);
}

Matching modified_worker_proposing_DA_algorithm(
    const int &n_agents,
    const int &n_firms,
    const std::vector<int> &firm_capacities,
    const std::vector<std::vector<int>> &agent_prefs,
    const std::vector<std::vector<int>> &firm_prefs,
    const std::vector<std::vector<int>> &agent_col_prefs,
    std::optional<unsigned int> seed,
    const DARandomChoiceSelector &choice_selector
)
{
    std::mt19937 rng(seed.has_value() ? *seed : std::random_device{}());
    std::vector<std::vector<int>> worker_order(n_agents);
    for (int worker = 0; worker < n_agents; ++worker) {
        auto &order = worker_order[worker];
        order.resize(n_firms);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int left, int right) {
            if (agent_prefs[worker][left] == agent_prefs[worker][right]) return left < right;
            return agent_prefs[worker][left] > agent_prefs[worker][right];
        });
    }

    std::vector<std::set<int>> firm_match(n_firms);
    std::vector<int> agent_match(n_agents, UNMATCHED);
    std::vector<int> next_idx(n_agents, 0);
    std::vector<std::vector<bool>> deleted(n_firms, std::vector<bool>(n_agents, false));
    int round = 0;

    while (true) {
        // A: Only workers unmatched at the START of the round may propose.
        // Firms decide after collecting all offers and their current incumbents.
        std::vector<std::vector<int>> candidates(n_firms);
        for (int firm = 0; firm < n_firms; ++firm) {
            candidates[firm].assign(firm_match[firm].begin(), firm_match[firm].end());
        }
        bool made_proposal = false;
        for (int worker = 0; worker < n_agents; ++worker) {
            if (agent_match[worker] != UNMATCHED) continue;
            while (next_idx[worker] < n_firms
                   && deleted[worker_order[worker][next_idx[worker]]][worker]) ++next_idx[worker];
            if (next_idx[worker] == n_firms) continue;

            const int firm = worker_order[worker][next_idx[worker]++];
            if (agent_prefs[worker][firm] < 0) {
                next_idx[worker] = n_firms;
                continue;
            }
            made_proposal = true;
            if (firm_prefs[firm][worker] < 0) continue;
            candidates[firm].push_back(worker);
        }
        if (!made_proposal) break;

        std::vector<int> next_agent_match(n_agents, UNMATCHED);
        for (int firm = 0; firm < n_firms; ++firm) {
            auto &workers = candidates[firm];
            std::sort(workers.begin(), workers.end(), [&](int left, int right) {
                if (firm_prefs[firm][left] != firm_prefs[firm][right]) {
                    return firm_prefs[firm][left] > firm_prefs[firm][right];
                }
                const bool left_incumbent = agent_match[left] == firm;
                const bool right_incumbent = agent_match[right] == firm;
                if (left_incumbent != right_incumbent) return left_incumbent;
                return left < right;
            });
            firm_match[firm].clear();
            for (int worker : workers) {
                if (static_cast<int>(firm_match[firm].size()) >= firm_capacities[firm]) break;
                firm_match[firm].insert(worker);
                next_agent_match[worker] = firm;
            }
        }
        agent_match = std::move(next_agent_match);

        // B: Permanently exclude the chosen worker only from this firm.
        resolve_colleague_conflicts(firm_match, agent_match, deleted, agent_col_prefs, rng, ++round, choice_selector);
    }

    return Matching::from_firm_assignment(firm_match, n_agents);
}
