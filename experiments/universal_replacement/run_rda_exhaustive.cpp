#include "rda_exhaustive.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <tuple>

namespace fs = std::filesystem;
using namespace rda_exhaustive;

namespace {
constexpr const char *summary_header =
    "scope,forbidden_pairs,initial_conflicts,inputs,all_stable,mixed,none_stable,"
    "nodes,branches,leaves,stable_leaves,distinct_matchings,distinct_stable_matchings,max_nodes,max_deletions\n";

Count unsigned_value(const std::string &value)
{
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("expected an unsigned integer: " + value);
    }
    return std::stoull(value);
}

std::ofstream output_file(const fs::path &path)
{
    std::ofstream out;
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out.open(path);
    out << std::setprecision(12);
    return out;
}

template<class T>
void array_json(std::ostream &out, const std::vector<T> &values, int offset = 0)
{
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << values[i] + offset;
    }
    out << ']';
}

void matrix_json(std::ostream &out, const Rankings &values)
{
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        array_json(out, values[i]);
    }
    out << ']';
}

void witness_json(std::ostream &out, const DASearchTree &tree, std::optional<std::size_t> leaf)
{
    if (!leaf) { out << "null"; return; }
    const auto &node = tree.nodes.at(*leaf);
    out << "{\"leaf_id\":" << *leaf << ",\"assignment\":";
    array_json(out, node.matching.get_agent_matchs(), 1);
    out << ",\"original_ur_stable\":" << (node.original_ur_stable ? "true" : "false")
        << ",\"deletions\":[";
    for (std::size_t i = 0; i < node.deletions.size(); ++i) {
        if (i) out << ',';
        out << "{\"firm\":" << node.deletions[i].firm + 1
            << ",\"worker\":" << node.deletions[i].worker + 1 << '}';
    }
    out << "],\"blocking_pairs\":[";
    for (std::size_t i = 0; i < node.original_blocking_pairs.size(); ++i) {
        if (i) out << ',';
        out << "{\"worker\":" << node.original_blocking_pairs[i].first + 1
            << ",\"firm\":" << node.original_blocking_pairs[i].second + 1 << '}';
    }
    out << "]}";
}

void example_json(std::ostream &out, Count id, const Problem &problem,
                  const DASearchTree &tree, const Summary &summary)
{
    out << "{\"instance_id\":" << id << ",\"capacities\":";
    array_json(out, problem.capacities);
    out << ",\"worker_orders\":"; matrix_json(out, problem.worker_rankings);
    out << ",\"firm_orders\":"; matrix_json(out, problem.firm_rankings);
    out << ",\"agent_prefs\":"; matrix_json(out, problem.agent_prefs);
    out << ",\"firm_prefs\":"; matrix_json(out, problem.firm_prefs);
    out << ",\"forbidden_pairs\":[";
    for (std::size_t i = 0; i < problem.forbidden_pairs.size(); ++i) {
        if (i) out << ',';
        out << '[' << problem.forbidden_pairs[i].first << ',' << problem.forbidden_pairs[i].second << ']';
    }
    out << "],\"initial_conflicts\":" << summary.initial_conflicts
        << ",\"category\":\"" << summary.category() << "\",\"nodes\":" << summary.nodes
        << ",\"leaves\":" << summary.leaves << ",\"stable_leaves\":" << summary.stable_leaves
        << ",\"stable_witness\":";
    witness_json(out, tree, summary.stable_witness);
    out << ",\"unstable_witness\":";
    witness_json(out, tree, summary.unstable_witness);
    out << "}\n";
}

void totals_csv(std::ostream &out, const Totals &totals)
{
    out << totals.inputs << ',' << totals.all_stable << ',' << totals.mixed << ',' << totals.none_stable
        << ',' << totals.nodes << ',' << totals.branches << ',' << totals.leaves << ',' << totals.stable_leaves
        << ',' << totals.distinct_matchings << ',' << totals.distinct_stable_matchings
        << ',' << totals.max_nodes << ',' << totals.max_deletions << '\n';
}

void save_summary(const fs::path &output, const Totals &overall,
                  const std::map<std::pair<int, int>, Totals> &groups)
{
    auto out = output_file(output / "summary.csv.tmp");
    out << summary_header << "overall,-1,-1,";
    totals_csv(out, overall);
    for (const auto &[key, totals] : groups) {
        out << "by_conflicts_and_forbidden," << key.first << ',' << key.second << ',';
        totals_csv(out, totals);
    }
    out.close();
    fs::rename(output / "summary.csv.tmp", output / "summary.csv");
}

void help()
{
    std::cout <<
        "Usage: run_rda_exhaustive --workers 3|4 --output NEW_DIRECTORY [options]\n"
        "  --workers 3          Two firms, three workers, capacities [2,1]\n"
        "  --workers 4          Two firms, four workers, capacities [2,2]\n"
        "  --start-id N         First input ID, inclusive (default: 0)\n"
        "  --count N            Number of inputs (default: all remaining inputs)\n"
        "  --progress-every N   Save progress and partial summary every N inputs (default: 10000)\n"
        "  --write-profiles     Also write one profiles.csv row per input\n"
        "  --dry-run            Print domain/range counts without creating output\n"
        "Full strict orders include outside option 0; all symmetric forbidden-pair masks.\n"
        "Firm-proposing recursive DA explores every branch, including duplicate paths.\n";
}
} // namespace

int main(int argc, char **argv)
{
    try {
        int workers = 0;
        Count start = 0, progress_every = 10000;
        std::optional<Count> count;
        bool write_profiles = false, dry_run = false;
        fs::path output;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") { help(); return 0; }
            if (option == "--write-profiles") { write_profiles = true; continue; }
            if (option == "--dry-run") { dry_run = true; continue; }
            if (i + 1 == argc) throw std::invalid_argument("missing value for " + option);
            const std::string value = argv[++i];
            if (option == "--workers") {
                if (value != "3" && value != "4") throw std::invalid_argument("--workers must be 3 or 4");
                workers = value == "3" ? 3 : 4;
            }
            else if (option == "--output") output = value;
            else if (option == "--start-id") start = unsigned_value(value);
            else if (option == "--count") count = unsigned_value(value);
            else if (option == "--progress-every") progress_every = unsigned_value(value);
            else throw std::invalid_argument("unknown option: " + option);
        }
        const Domain domain(workers);
        const auto selected = range(domain, start, count);
        if (progress_every == 0) throw std::invalid_argument("--progress-every must be positive");
        std::cout << "workers=" << workers << " capacities=[2," << domain.capacities[1]
                  << "] total_inputs=" << domain.total << " selected_range=[" << selected.begin
                  << ',' << selected.end << ") count=" << selected.size() << std::endl;
        if (dry_run) return 0;
        if (output.empty()) throw std::invalid_argument("--output is required");
        if (!output.parent_path().empty()) fs::create_directories(output.parent_path());
        if (!fs::create_directory(output)) throw std::runtime_error("output directory already exists");
        {
            auto config = output_file(output / "config.json");
            config << "{\"schema_version\":1,\"experiment\":\"rda_exhaustive_strict_outside_v1\","
                   << "\"proposing_side\":\"firm\",\"n_workers\":" << workers
                   << ",\"n_firms\":2,\"capacities\":";
            array_json(config, domain.capacities);
            config << ",\"total_domain_inputs\":" << domain.total << ",\"start_id\":" << selected.begin
                   << ",\"end_id_exclusive\":" << selected.end << ",\"selected_inputs\":" << selected.size()
                   << ",\"worker_order_count\":" << domain.worker_orders.size()
                   << ",\"firm_order_count\":" << domain.firm_orders.size()
                   << ",\"forbidden_masks\":" << domain.masks
                   << ",\"order_labels\":\"one_based_counterparts_zero_outside\","
                   << "\"enumeration\":\"lexicographic_orders_workers_then_firms_then_forbidden_mask_fastest\","
                   << "\"outside_score\":0,\"score_rule\":\"outside_rank_minus_counterpart_rank\","
                   << "\"symmetric_forbidden_pairs\":true,\"full_path_enumeration\":true,"
                   << "\"duplicate_branches_kept\":true,\"early_stopping\":false,"
                   << "\"vacancy_blocking\":true,\"stability_against_original_preferences\":true,"
                   << "\"write_profiles\":" << (write_profiles ? "true" : "false") << "}\n";
            config.close();
        }

        auto examples = output_file(output / "examples.jsonl");
        std::ofstream profiles;
        if (write_profiles) {
            profiles = output_file(output / "profiles.csv");
            profiles << "instance_id,forbidden_pairs,initial_conflicts,category,nodes,branches,leaves,"
                     << "stable_leaves,distinct_matchings,distinct_stable_matchings,max_deletions,"
                     << "min_matched_workers,max_matched_workers\n";
        }
        Totals overall;
        std::map<std::pair<int, int>, Totals> groups;
        std::set<std::tuple<int, int, std::string>> example_groups;
        const auto started = std::chrono::steady_clock::now();
        const auto elapsed = [&]() {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        };
        const auto progress = [&](Count next_id) {
            examples.flush();
            if (write_profiles) profiles.flush();
            save_summary(output, overall, groups);
            auto status = output_file(output / "progress.json.tmp");
            status << "{\"processed_inputs\":" << overall.inputs << ",\"next_id\":" << next_id
                   << ",\"elapsed_seconds\":" << elapsed() << "}\n";
            status.close();
            fs::rename(output / "progress.json.tmp", output / "progress.json");
            std::cout << "processed=" << overall.inputs << '/' << selected.size() << " next_id=" << next_id
                      << " all_stable=" << overall.all_stable << " mixed=" << overall.mixed
                      << " none_stable=" << overall.none_stable << " nodes=" << overall.nodes
                      << " elapsed_seconds=" << elapsed() << std::endl;
        };

        for (Count id = selected.begin; id < selected.end; ++id) {
            const auto problem = domain.problem(id);
            const auto tree = recursive_firm_proposing_DA_algorithm(workers, 2, domain.capacities,
                problem.agent_prefs, problem.firm_prefs, problem.colleague_prefs);
            const auto summary = rda_batch::summarize(tree, problem);
            const int pairs = static_cast<int>(problem.forbidden_pairs.size());
            const int conflicts = static_cast<int>(summary.initial_conflicts);
            overall.add(summary);
            groups[{pairs, conflicts}].add(summary);
            if (example_groups.emplace(pairs, conflicts, summary.category()).second) {
                example_json(examples, id, problem, tree, summary);
            }
            if (write_profiles) {
                profiles << id << ',' << pairs << ',' << conflicts << ',' << summary.category()
                         << ',' << summary.nodes << ',' << summary.branches << ',' << summary.leaves
                         << ',' << summary.stable_leaves << ',' << summary.distinct_matchings
                         << ',' << summary.distinct_stable_matchings << ',' << summary.max_deletions
                         << ',' << summary.min_matched_workers << ',' << summary.max_matched_workers << '\n';
            }
            if (overall.inputs % progress_every == 0 || id + 1 == selected.end) progress(id + 1);
        }
        examples.close();
        if (write_profiles) profiles.close();
        auto completion = output_file(output / "completion.json");
        completion << "{\"complete\":true,\"whole_domain_complete\":"
                   << (selected.begin == 0 && selected.end == domain.total ? "true" : "false")
                   << ",\"processed_inputs\":" << overall.inputs << ",\"next_id\":" << selected.end
                   << ",\"nodes\":" << overall.nodes << ",\"elapsed_seconds\":" << elapsed() << "}\n";
        completion.close();
        std::cout << "Outputs: " << fs::absolute(output) << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
