// Persistent evaluator. The Python CLI owns enumeration, aggregation and checkpoints;
// this process uses the existing C++ RDA and Matching predicates unchanged.
#include "rda_batch.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <sstream>

using universal_replacement_experiment::Problem;
using Flags = std::array<bool, 3>;

namespace {
template<class T> void array(std::ostream &out, const std::vector<T> &v, int offset = 0)
{
    out << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out << ',';
        out << v[i] + offset;
    }
    out << ']';
}

Flags evaluate(Matching matching, const Problem &p)
{
    // Always evaluate against original preferences. Each predicate sees the same
    // scores; no reduced preference matrix is passed to this function.
    return {matching.is_universal_replacement_stable(p.agent_prefs, p.firm_prefs, p.colleague_prefs, p.capacities),
            matching.is_weakly_stable(p.agent_prefs, p.firm_prefs, p.colleague_prefs, p.capacities),
            matching.is_stable(p.agent_prefs, p.firm_prefs, p.colleague_prefs, p.capacities)};
}

int mask(const Flags &flags)
{
    return int(flags[0]) | (int(flags[1]) << 1) | (int(flags[2]) << 2);
}

void flags_json(std::ostream &out, const Flags &flags)
{
    out << '[' << flags[0] << ',' << flags[1] << ',' << flags[2] << ']';
}

void pairs_json(std::ostream &out, const std::vector<std::pair<int, int>> &pairs)
{
    out << '[';
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        if (i) out << ',';
        out << '[' << pairs[i].first + 1 << ',' << pairs[i].second + 1 << ']';
    }
    out << ']';
}

// A retained-incumbent set makes general-stability blockers reproducible.
void blocking_subset(std::ostream &out, Matching matching, const Problem &p,
                     const std::pair<int, int> &pair)
{
    const auto [worker, firm] = pair;
    const auto &incumbents = matching.get_firm_matchs()[firm];
    const auto &agent_scores = matching.get_agent_scores();
    const auto &firm_scores = matching.get_firm_scores();
    std::vector<int> members(incumbents.begin(), incumbents.end());
    for (unsigned bits = 0; bits < (1U << members.size()); ++bits) {
        std::set<int> after;
        for (std::size_t i = 0; i < members.size(); ++i)
            if (bits & (1U << i)) after.insert(members[i]);
        if (after.size() + 1 > static_cast<std::size_t>(p.capacities[firm])) continue;
        after.insert(worker);
        if (compute_firm_score(after, p.firm_prefs[firm]) <= firm_scores[firm]) continue;
        if (compute_agent_score(firm, after, p.agent_prefs[worker], p.colleague_prefs[worker])
            <= agent_scores[worker]) continue;
        bool acceptable = true;
        for (int colleague : after) {
            if (colleague != worker && compute_agent_score(firm, after,
                p.agent_prefs[colleague], p.colleague_prefs[colleague]) < agent_scores[colleague]) {
                acceptable = false;
                break;
            }
        }
        if (!acceptable) continue;
        after.erase(worker);
        array(out, std::vector<int>(after.begin(), after.end()), 1);
        return;
    }
    throw std::logic_error("blocking pair has no witnessing subset");
}

void matching_json(std::ostream &out, Matching matching, const Problem &p)
{
    const auto flags = evaluate(matching, p);
    out << "{\"assignment\":";
    array(out, matching.get_agent_matchs(), 1);
    out << ",\"stability\":";
    flags_json(out, flags);
    auto ur = matching.universal_replacement_blocking_pairs(p.agent_prefs, p.firm_prefs, p.colleague_prefs, p.capacities);
    auto weak = matching.weakly_blocking_pairs(p.agent_prefs, p.firm_prefs, p.colleague_prefs, p.capacities);
    auto stable = matching.blocking_pairs(p.agent_prefs, p.firm_prefs, p.colleague_prefs, p.capacities);
    out << ",\"blocking_pairs\":{\"ur\":"; pairs_json(out, ur);
    out << ",\"weak\":"; pairs_json(out, weak);
    out << ",\"stability\":"; pairs_json(out, stable);
    out << "},\"stability_blocking_witness\":";
    if (stable.empty()) out << "null";
    else {
        out << "{\"worker\":" << stable[0].first + 1 << ",\"firm\":" << stable[0].second + 1
            << ",\"retained_workers\":";
        blocking_subset(out, matching, p, stable[0]);
        out << '}';
    }
    out << '}';
}

void run(std::ostream &out, unsigned long long id, const Problem &p)
{
    const int m = static_cast<int>(p.agent_prefs.size());
    const int n = static_cast<int>(p.capacities.size());
    const auto started = std::chrono::steady_clock::now();
    const auto tree = recursive_firm_proposing_DA_algorithm(m, n, p.capacities,
        p.agent_prefs, p.firm_prefs, p.colleague_prefs);
    std::array<unsigned long long, 8> leaf_patterns{}, distinct_patterns{};
    std::array<unsigned long long, 3> stable_leaves{}, stable_distinct{}, existence_counts{};
    std::array<std::optional<Matching>, 3> existence_witnesses;
    std::array<std::optional<std::size_t>, 6> leaf_witnesses;
    std::set<std::vector<int>> distinct;
    std::size_t max_depth = 0;
    for (const auto &node : tree.nodes) max_depth = std::max(max_depth, node.deletions.size());
    for (auto leaf : tree.leaves) {
        const auto &matching = tree.nodes[leaf].matching;
        const auto flags = evaluate(matching, p);
        ++leaf_patterns[mask(flags)];
        const bool first = distinct.insert(matching.get_agent_matchs()).second;
        if (first) ++distinct_patterns[mask(flags)];
        for (int d = 0; d < 3; ++d) {
            stable_leaves[d] += flags[d];
            stable_distinct[d] += flags[d] && first;
            if (!leaf_witnesses[2 * d + flags[d]]) leaf_witnesses[2 * d + flags[d]] = leaf;
        }
    }

    const auto rda_finished = std::chrono::steady_clock::now();
    // Enumerate EVERY capacity-respecting assignment, including unmatched.
    // No successful-existence early exit; record all three stable-solution counts.
    unsigned long long candidates = 0, feasible_candidates = 0;
    std::vector<std::set<int>> teams(n);
    std::function<void(int)> visit = [&](int worker) {
        if (worker != m) {
            visit(worker + 1);
            for (int firm = 0; firm < n; ++firm) {
                if (teams[firm].size() >= static_cast<std::size_t>(p.capacities[firm])) continue;
                teams[firm].insert(worker);
                visit(worker + 1);
                teams[firm].erase(worker);
            }
            return;
        }
        ++candidates;
        for (const auto &team : teams)
            for (int a : team) for (int b : team)
                if (a != b && (p.colleague_prefs[a][b] == -MatchingTypes::INF
                    || p.colleague_prefs[b][a] == -MatchingTypes::INF)) return;
        auto matching = Matching::from_firm_assignment(teams, m);
        // Common prerequisites to all three existing predicates.
        if (!matching.is_individually_rational(p.agent_prefs, p.firm_prefs, p.colleague_prefs)) return;
        ++feasible_candidates;
        const auto flags = evaluate(matching, p);
        for (int d = 0; d < 3; ++d) {
            if (!flags[d]) continue;
            ++existence_counts[d];
            if (!existence_witnesses[d]) existence_witnesses[d] = matching;
        }
    };
    visit(0);
    const auto existence_finished = std::chrono::steady_clock::now();
    for (int d = 0; d < 3; ++d)
        if (stable_distinct[d] > existence_counts[d]) throw std::logic_error("RDA outputs exceed stable solutions");

    out << "{\"instance_id\":" << id << ",\"nodes\":" << tree.nodes.size()
        << ",\"branches\":" << tree.branches.size() << ",\"leaves\":" << tree.leaves.size()
        << ",\"distinct_matchings\":" << distinct.size() << ",\"max_deletions\":" << max_depth
        << ",\"initial_conflicts\":" << tree.nodes[0].conflicts.size()
        << ",\"assignment_candidates\":" << candidates << ",\"evaluated_assignments\":" << feasible_candidates
        << ",\"rda_seconds\":" << std::chrono::duration<double>(rda_finished - started).count()
        << ",\"existence_seconds\":" << std::chrono::duration<double>(existence_finished - rda_finished).count();
    out << ",\"stable_leaves\":[" << stable_leaves[0] << ',' << stable_leaves[1] << ',' << stable_leaves[2] << ']';
    out << ",\"distinct_stable_matchings\":[" << stable_distinct[0] << ',' << stable_distinct[1] << ',' << stable_distinct[2] << ']';
    out << ",\"stable_solution_counts\":[" << existence_counts[0] << ',' << existence_counts[1] << ',' << existence_counts[2] << ']';
    out << ",\"leaf_patterns\":[";
    for (int i = 0; i < 8; ++i) { if (i) out << ','; out << leaf_patterns[i]; }
    out << "],\"distinct_patterns\":[";
    for (int i = 0; i < 8; ++i) { if (i) out << ','; out << distinct_patterns[i]; }
    out << "],\"existence_witnesses\":[";
    for (int d = 0; d < 3; ++d) {
        if (d) out << ',';
        if (existence_witnesses[d]) matching_json(out, *existence_witnesses[d], p);
        else out << "null";
    }
    std::set<std::size_t> witness_ids;
    for (const auto &leaf : leaf_witnesses) if (leaf) witness_ids.insert(*leaf);
    out << "],\"leaf_witnesses\":[";
    bool first_leaf = true;
    for (auto leaf : witness_ids) {
        if (!first_leaf) out << ',';
        first_leaf = false;
        out << "{\"leaf_id\":" << leaf << ",\"matching\":";
        matching_json(out, tree.nodes[leaf].matching, p);
        out << ",\"path\":[";
        const auto path = rda_batch::history(tree, leaf);
        for (std::size_t i = 0; i < path.size(); ++i) {
            if (i) out << ',';
            const auto &edge = tree.branches[path[i]];
            const auto &parent = tree.nodes[edge.from];
            const auto &conflict = parent.conflicts[edge.conflict_index];
            out << "{\"firm\":" << conflict.firm + 1 << ",\"forbidden_pair\":["
                << conflict.left + 1 << ',' << conflict.right + 1 << "],\"removed_worker\":"
                << edge.removed_worker + 1 << ",\"conflict_index\":" << edge.conflict_index
                << ",\"parent_assignment\":";
            array(out, parent.matching.get_agent_matchs(), 1);
            out << ",\"child_assignment\":";
            array(out, tree.nodes[*edge.to].matching.get_agent_matchs(), 1);
            out << '}';
        }
        out << "]}";
    }
    out << "]}\n" << std::flush;
}
} // namespace

int main(int argc, char **argv)
{
    try {
        std::cout << std::boolalpha;
        if (argc < 4) throw std::invalid_argument("engine requires FIRMS WORKERS CAPACITY...");
        const int n = std::stoi(argv[1]), m = std::stoi(argv[2]);
        if (n < 1 || n > 3 || m < 1 || m > 5 || argc != n + 3)
            throw std::invalid_argument("invalid engine dimensions");
        Problem p;
        for (int f = 0; f < n; ++f) {
            const int c = std::stoi(argv[3 + f]);
            if (c < 0 || c > m) throw std::invalid_argument("invalid capacity");
            p.capacities.push_back(c);
        }
        p.agent_prefs.assign(m, std::vector<int>(n));
        p.firm_prefs.assign(n, std::vector<int>(m));
        p.colleague_prefs.assign(m, std::vector<int>(m));
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream input(line);
            unsigned long long id;
            if (!(input >> id)) throw std::invalid_argument("missing input ID");
            for (auto *matrix : {&p.agent_prefs, &p.firm_prefs, &p.colleague_prefs})
                for (auto &row : *matrix) for (auto &score : row)
                    if (!(input >> score)) throw std::invalid_argument("truncated score matrices");
            std::string extra;
            if (input >> extra) throw std::invalid_argument("extra input fields");
            run(std::cout, id, p);
        }
        if (!std::cin.eof()) throw std::runtime_error("engine input error");
    } catch (const std::exception &error) {
        std::cerr << "Engine error: " << error.what() << '\n';
        return 1;
    }
}
