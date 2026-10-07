#include "simulation_common.hpp"

#include "CommonTypes.hpp"
#include "Matching.hpp"
#include "MatchingSystem.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace stability_simulation {
namespace {

using MatchingTypes::INF;
using MatchingTypes::UNMATCHED;

struct MatchingRecord {
    std::vector<std::vector<int>> firms;
    std::vector<int> unmatched;
};

struct AlgorithmResult {
    MatchingRecord matching;
    bool is_stable = false;
    bool is_weakly_stable = false;
};

struct SearchResult {
    std::uint64_t checked_matching_count = 0;
    std::uint64_t stable_matching_count = 0;
    std::uint64_t weakly_stable_matching_count = 0;
    std::optional<MatchingRecord> stable_witness;
    std::optional<MatchingRecord> weakly_stable_witness;
    std::uint64_t stable_not_weakly_matching_count = 0;
};

struct CategoryCounts {
    std::uint64_t both_success = 0;
    std::uint64_t simple_only = 0;
    std::uint64_t doctor_only = 0;
    std::uint64_t both_missed = 0;
    std::uint64_t no_solution = 0;
};

struct KStats {
    std::uint64_t problems = 0;
    std::uint64_t stable_exists = 0;
    std::uint64_t weakly_stable_exists = 0;
    std::uint64_t simple_stable = 0;
    std::uint64_t doctor_stable = 0;
    std::uint64_t simple_weakly_stable = 0;
    std::uint64_t doctor_weakly_stable = 0;
};

struct Summary {
    std::uint64_t total = 0;
    std::uint64_t stable_exists = 0;
    std::uint64_t weakly_stable_exists = 0;
    std::uint64_t simple_stable = 0;
    std::uint64_t doctor_stable = 0;
    std::uint64_t simple_weakly_stable = 0;
    std::uint64_t doctor_weakly_stable = 0;
    std::uint64_t warning_instances = 0;
    std::uint64_t stable_not_weakly_candidate_count = 0;
    std::uint64_t simple_stable_not_weakly = 0;
    std::uint64_t doctor_stable_not_weakly = 0;
    CategoryCounts stable_categories;
    CategoryCounts weak_categories;
    std::map<std::uint64_t, std::uint64_t> stable_count_distribution;
    std::map<std::uint64_t, std::uint64_t> weak_count_distribution;
    std::map<int, KStats> by_rejection_count;
    double elapsed_seconds = 0.0;
};

std::uint64_t splitmix64(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::vector<std::vector<int>> permutations_of_scores(int size)
{
    std::vector<int> scores(size);
    std::iota(scores.begin(), scores.end(), 1);
    std::vector<std::vector<int>> result;
    do {
        result.push_back(scores);
    } while (std::next_permutation(scores.begin(), scores.end()));
    return result;
}

std::vector<int> random_strict_scores(int size, std::mt19937_64 &rng)
{
    std::vector<int> order(size);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);

    std::vector<int> scores(size, 0);
    for (int rank = 0; rank < size; ++rank) {
        scores[order[rank]] = size - rank;
    }
    return scores;
}

void enumerate_assignments_recursive(
    int worker,
    int n_workers,
    const std::vector<int> &capacities,
    std::vector<int> &used,
    std::vector<int> &current,
    std::vector<std::vector<int>> &result
)
{
    if (worker == n_workers) {
        result.push_back(current);
        return;
    }

    current[worker] = UNMATCHED;
    enumerate_assignments_recursive(
        worker + 1, n_workers, capacities, used, current, result);

    for (int firm = 0; firm < static_cast<int>(capacities.size()); ++firm) {
        if (used[firm] >= capacities[firm]) continue;
        current[worker] = firm;
        ++used[firm];
        enumerate_assignments_recursive(
            worker + 1, n_workers, capacities, used, current, result);
        --used[firm];
    }
}

Matching matching_from_assignment(
    const std::vector<int> &assignment,
    int n_firms
)
{
    std::vector<std::set<int>> firm_matches(n_firms);
    for (int worker = 0; worker < static_cast<int>(assignment.size()); ++worker) {
        const int firm = assignment[worker];
        if (firm != UNMATCHED) firm_matches[firm].insert(worker);
    }
    return Matching::from_firm_assignment(firm_matches, assignment.size());
}

MatchingRecord matching_record(const Matching &matching)
{
    MatchingRecord record;
    for (const auto &firm_match : matching.get_firm_matchs()) {
        record.firms.emplace_back(firm_match.begin(), firm_match.end());
    }
    const auto &worker_matches = matching.get_agent_matchs();
    for (int worker = 0; worker < static_cast<int>(worker_matches.size()); ++worker) {
        if (worker_matches[worker] == UNMATCHED) record.unmatched.push_back(worker);
    }
    return record;
}

bool evaluate_stable(Matching &matching, const Instance &instance, const CaseSpec &spec)
{
    return matching.is_stable(
        instance.worker_prefs,
        instance.firm_prefs,
        instance.colleague_prefs,
        spec.capacities
    );
}

bool evaluate_weakly_stable(
    Matching &matching,
    const Instance &instance,
    const CaseSpec &spec
)
{
    return matching.is_weakly_stable(
        instance.worker_prefs,
        instance.firm_prefs,
        instance.colleague_prefs,
        spec.capacities
    );
}

SearchResult exhaustive_search(
    const Instance &instance,
    const CaseSpec &spec,
    const std::vector<std::vector<int>> &assignments
)
{
    SearchResult result;
    for (const auto &assignment : assignments) {
        // A new object is required for every profile because Matching caches scores.
        Matching candidate = matching_from_assignment(assignment, spec.n_firms);
        const bool stable = evaluate_stable(candidate, instance, spec);
        const bool weakly_stable = evaluate_weakly_stable(candidate, instance, spec);
        ++result.checked_matching_count;
        if (stable) {
            ++result.stable_matching_count;
            if (!result.stable_witness) result.stable_witness = matching_record(candidate);
        }
        if (weakly_stable) {
            ++result.weakly_stable_matching_count;
            if (!result.weakly_stable_witness) {
                result.weakly_stable_witness = matching_record(candidate);
            }
        }
        if (stable && !weakly_stable) {
            ++result.stable_not_weakly_matching_count;
        }
    }
    return result;
}

AlgorithmResult evaluate_algorithm(
    Matching matching,
    const Instance &instance,
    const CaseSpec &spec
)
{
    AlgorithmResult result;
    result.matching = matching_record(matching);
    result.is_stable = evaluate_stable(matching, instance, spec);
    result.is_weakly_stable = evaluate_weakly_stable(matching, instance, spec);
    return result;
}

int rejection_count(const Instance &instance)
{
    int count = 0;
    for (int i = 0; i < static_cast<int>(instance.colleague_prefs.size()); ++i) {
        for (int j = 0; j < static_cast<int>(instance.colleague_prefs.size()); ++j) {
            if (i != j && instance.colleague_prefs[i][j] == -INF) ++count;
        }
    }
    return count;
}

void json_string(std::ostream &out, const std::string &value)
{
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c) << std::dec << std::setfill(' ');
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
}

template <typename T>
void json_vector(std::ostream &out, const std::vector<T> &values)
{
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << values[i];
    }
    out << ']';
}

template <typename T>
void json_matrix(std::ostream &out, const std::vector<std::vector<T>> &matrix)
{
    out << '[';
    for (std::size_t i = 0; i < matrix.size(); ++i) {
        if (i) out << ',';
        json_vector(out, matrix[i]);
    }
    out << ']';
}

void json_bool(std::ostream &out, bool value)
{
    out << (value ? "true" : "false");
}

void json_matching(std::ostream &out, const MatchingRecord &matching)
{
    out << "{\"firms\":";
    json_matrix(out, matching.firms);
    out << ",\"unmatched\":";
    json_vector(out, matching.unmatched);
    out << '}';
}

void json_algorithm(std::ostream &out, const AlgorithmResult &result)
{
    out << "{\"matching\":";
    json_matching(out, result.matching);
    out << ",\"is_stable\":";
    json_bool(out, result.is_stable);
    out << ",\"is_weakly_stable\":";
    json_bool(out, result.is_weakly_stable);
    out << '}';
}

std::string instance_id(const CaseSpec &spec, std::uint64_t index)
{
    std::ostringstream out;
    out << spec.name << '-' << std::setw(6) << std::setfill('0') << index;
    return out.str();
}

void write_instance_json(
    std::ostream &out,
    const CaseSpec &spec,
    std::uint64_t run_seed,
    std::uint64_t index,
    std::uint32_t priority_seed,
    const Instance &instance,
    int rejections,
    const AlgorithmResult &simple,
    const AlgorithmResult &doctor,
    const SearchResult &search,
    const std::vector<std::string> *reasons
)
{
    const bool random = spec.mode == "random";
    const double density = static_cast<double>(rejections)
        / static_cast<double>(spec.n_workers * (spec.n_workers - 1));

    out << "{\"instance_id\":";
    json_string(out, instance_id(spec, index));
    out << ",\"mode\":";
    json_string(out, spec.mode);
    out << ",\"seed\":";
    if (random) out << run_seed; else out << "null";
    out << ",\"trial\":";
    if (random) out << index; else out << "null";
    out << ",\"doctor_priority_seed\":" << priority_seed;
    out << ",\"n_firms\":" << spec.n_firms;
    out << ",\"n_workers\":" << spec.n_workers;
    out << ",\"capacities\":";
    json_vector(out, spec.capacities);
    out << ",\"firm_scores\":";
    json_string(out, spec.firm_scores);
    out << ",\"worker_preferences\":";
    json_matrix(out, instance.worker_prefs);
    out << ",\"firm_preferences\":";
    json_matrix(out, instance.firm_prefs);
    out << ",\"colleague_preferences\":";
    json_matrix(out, instance.colleague_prefs);
    out << ",\"colleague_rejection_count\":" << rejections;
    out << ",\"colleague_rejection_density\":"
        << std::setprecision(17) << density;
    out << ",\"simple_match\":";
    json_algorithm(out, simple);
    out << ",\"doctor_dictator\":";
    json_algorithm(out, doctor);
    out << ",\"full_search\":{\"checked_matching_count\":"
        << search.checked_matching_count
        << ",\"stable_matching_count\":" << search.stable_matching_count
        << ",\"weakly_stable_matching_count\":"
        << search.weakly_stable_matching_count
        << ",\"stable_exists\":";
    json_bool(out, search.stable_matching_count > 0);
    out << ",\"weakly_stable_exists\":";
    json_bool(out, search.weakly_stable_matching_count > 0);
    out << ",\"stable_witness\":";
    if (search.stable_witness) json_matching(out, *search.stable_witness);
    else out << "null";
    out << ",\"weakly_stable_witness\":";
    if (search.weakly_stable_witness) {
        json_matching(out, *search.weakly_stable_witness);
    } else {
        out << "null";
    }
    out << ",\"stable_not_weakly_matching_count\":"
        << search.stable_not_weakly_matching_count << '}';

    out << ",\"consistency_warnings\":[";
    bool first_warning = true;
    const auto warning = [&](const char *name, bool active) {
        if (!active) return;
        if (!first_warning) out << ',';
        json_string(out, name);
        first_warning = false;
    };
    warning("simple_stable_without_weakly_stable",
        simple.is_stable && !simple.is_weakly_stable);
    warning("doctor_stable_without_weakly_stable",
        doctor.is_stable && !doctor.is_weakly_stable);
    warning("enumerated_stable_without_weakly_stable",
        search.stable_not_weakly_matching_count > 0);
    warning("stable_exists_without_weakly_stable_exists",
        search.stable_matching_count > 0 && search.weakly_stable_matching_count == 0);
    out << ']';

    if (reasons) {
        out << ",\"reasons\":[";
        for (std::size_t i = 0; i < reasons->size(); ++i) {
            if (i) out << ',';
            json_string(out, (*reasons)[i]);
        }
        out << ']';
    }
    out << "}\n";
}

void update_categories(
    CategoryCounts &categories,
    bool exists,
    bool simple_success,
    bool doctor_success
)
{
    if (!exists) {
        ++categories.no_solution;
    } else if (simple_success && doctor_success) {
        ++categories.both_success;
    } else if (simple_success) {
        ++categories.simple_only;
    } else if (doctor_success) {
        ++categories.doctor_only;
    } else {
        ++categories.both_missed;
    }
}

std::vector<std::string> interesting_reasons(
    const AlgorithmResult &simple,
    const AlgorithmResult &doctor,
    const SearchResult &search
)
{
    std::vector<std::string> reasons;
    const bool stable_exists = search.stable_matching_count > 0;
    const bool weak_exists = search.weakly_stable_matching_count > 0;
    if (stable_exists && !simple.is_stable) {
        reasons.push_back("simple_missed_existing_stable");
    }
    if (stable_exists && !doctor.is_stable) {
        reasons.push_back("doctor_missed_existing_stable");
    }
    if (weak_exists && !simple.is_weakly_stable) {
        reasons.push_back("simple_missed_existing_weakly_stable");
    }
    if (weak_exists && !doctor.is_weakly_stable) {
        reasons.push_back("doctor_missed_existing_weakly_stable");
    }
    if (!stable_exists) reasons.push_back("no_stable_matching");
    if (!weak_exists) reasons.push_back("no_weakly_stable_matching");
    if (simple.is_stable && !simple.is_weakly_stable) {
        reasons.push_back("simple_stable_without_weakly_stable");
    }
    if (doctor.is_stable && !doctor.is_weakly_stable) {
        reasons.push_back("doctor_stable_without_weakly_stable");
    }
    if (search.stable_not_weakly_matching_count > 0) {
        reasons.push_back("enumerated_stable_without_weakly_stable");
    }
    if (stable_exists && !weak_exists) {
        reasons.push_back("stable_exists_without_weakly_stable_exists");
    }
    return reasons;
}

void update_summary(
    Summary &summary,
    int rejections,
    const AlgorithmResult &simple,
    const AlgorithmResult &doctor,
    const SearchResult &search,
    bool warning_instance
)
{
    const bool stable_exists = search.stable_matching_count > 0;
    const bool weak_exists = search.weakly_stable_matching_count > 0;
    ++summary.total;
    summary.stable_exists += stable_exists;
    summary.weakly_stable_exists += weak_exists;
    summary.simple_stable += simple.is_stable;
    summary.doctor_stable += doctor.is_stable;
    summary.simple_weakly_stable += simple.is_weakly_stable;
    summary.doctor_weakly_stable += doctor.is_weakly_stable;
    summary.warning_instances += warning_instance;
    summary.stable_not_weakly_candidate_count +=
        search.stable_not_weakly_matching_count;
    summary.simple_stable_not_weakly +=
        simple.is_stable && !simple.is_weakly_stable;
    summary.doctor_stable_not_weakly +=
        doctor.is_stable && !doctor.is_weakly_stable;

    update_categories(
        summary.stable_categories, stable_exists, simple.is_stable, doctor.is_stable);
    update_categories(
        summary.weak_categories,
        weak_exists,
        simple.is_weakly_stable,
        doctor.is_weakly_stable
    );
    ++summary.stable_count_distribution[search.stable_matching_count];
    ++summary.weak_count_distribution[search.weakly_stable_matching_count];

    auto &by_k = summary.by_rejection_count[rejections];
    ++by_k.problems;
    by_k.stable_exists += stable_exists;
    by_k.weakly_stable_exists += weak_exists;
    by_k.simple_stable += simple.is_stable;
    by_k.doctor_stable += doctor.is_stable;
    by_k.simple_weakly_stable += simple.is_weakly_stable;
    by_k.doctor_weakly_stable += doctor.is_weakly_stable;
}

double rate(std::uint64_t numerator, std::uint64_t denominator)
{
    if (denominator == 0) return std::numeric_limits<double>::quiet_NaN();
    return static_cast<double>(numerator) / static_cast<double>(denominator);
}

void write_rate(std::ostream &out, std::uint64_t numerator, std::uint64_t denominator)
{
    out << numerator << '/' << denominator << " (";
    if (denominator == 0) out << "N/A";
    else out << std::fixed << std::setprecision(6) << rate(numerator, denominator);
    out << ')';
}

std::uint64_t category_total(const CategoryCounts &categories)
{
    return categories.both_success + categories.simple_only
        + categories.doctor_only + categories.both_missed + categories.no_solution;
}

void validate_summary(const Summary &summary)
{
    if (summary.stable_exists != summary.total - summary.stable_categories.no_solution
        || summary.weakly_stable_exists
            != summary.total - summary.weak_categories.no_solution) {
        throw std::logic_error("existence counts and no_solution categories disagree");
    }
    if (category_total(summary.stable_categories) != summary.total
        || category_total(summary.weak_categories) != summary.total) {
        throw std::logic_error("category counts do not sum to total problems");
    }
}

template <typename Key>
void write_distribution(
    std::ostream &out,
    const std::map<Key, std::uint64_t> &distribution
)
{
    bool first = true;
    for (const auto &[value, count] : distribution) {
        if (!first) out << ", ";
        out << value << ':' << count;
        first = false;
    }
}

void write_category_summary(
    std::ostream &out,
    const char *name,
    const CategoryCounts &categories
)
{
    out << name << " categories\n"
        << "  both_success: " << categories.both_success << '\n'
        << "  simple_only: " << categories.simple_only << '\n'
        << "  doctor_only: " << categories.doctor_only << '\n'
        << "  both_missed: " << categories.both_missed << '\n'
        << "  no_solution: " << categories.no_solution << '\n'
        << "  total: " << category_total(categories) << "\n\n";
}

void write_summary_txt(
    const std::filesystem::path &path,
    const CaseSpec &spec,
    std::uint64_t seed,
    const Summary &summary
)
{
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot open " + path.string());

    out << "Matching stability simulation\n"
        << "case: " << spec.name << '\n'
        << "mode: " << spec.mode << '\n'
        << "seed: " << seed << '\n'
        << "n_firms: " << spec.n_firms << '\n'
        << "n_workers: " << spec.n_workers << '\n'
        << "firm_scores: " << spec.firm_scores << '\n'
        << "capacities: ";
    for (std::size_t i = 0; i < spec.capacities.size(); ++i) {
        if (i) out << ',';
        out << spec.capacities[i];
    }
    out << "\nmatching_candidates_per_problem: " << spec.expected_matching_count
        << "\nproblems: " << summary.total << "\n\n";

    out << "Existence\n  stable_exists: ";
    write_rate(out, summary.stable_exists, summary.total);
    out << "\n  stable_exists=false: " << summary.total - summary.stable_exists
        << "\n  weakly_stable_exists: ";
    write_rate(out, summary.weakly_stable_exists, summary.total);
    out << "\n  weakly_stable_exists=false: "
        << summary.total - summary.weakly_stable_exists << "\n\n";

    out << "Algorithm output rates (denominator: all problems)\n"
        << "  simple stable: ";
    write_rate(out, summary.simple_stable, summary.total);
    out << "\n  doctor stable: ";
    write_rate(out, summary.doctor_stable, summary.total);
    out << "\n  simple weakly stable: ";
    write_rate(out, summary.simple_weakly_stable, summary.total);
    out << "\n  doctor weakly stable: ";
    write_rate(out, summary.doctor_weakly_stable, summary.total);
    out << "\n\nAlgorithm conditional derivation rates (denominator: problems with a solution)\n"
        << "  simple stable: ";
    write_rate(out, summary.simple_stable, summary.stable_exists);
    out << "\n  doctor stable: ";
    write_rate(out, summary.doctor_stable, summary.stable_exists);
    out << "\n  simple weakly stable: ";
    write_rate(out, summary.simple_weakly_stable, summary.weakly_stable_exists);
    out << "\n  doctor weakly stable: ";
    write_rate(out, summary.doctor_weakly_stable, summary.weakly_stable_exists);
    out << "\n\nMissed existing solutions\n"
        << "  simple_missed_existing_stable: "
        << summary.stable_exists - summary.simple_stable << '\n'
        << "  doctor_missed_existing_stable: "
        << summary.stable_exists - summary.doctor_stable << '\n'
        << "  simple_missed_existing_weakly_stable: "
        << summary.weakly_stable_exists - summary.simple_weakly_stable << '\n'
        << "  doctor_missed_existing_weakly_stable: "
        << summary.weakly_stable_exists - summary.doctor_weakly_stable
        << "\n\n";

    write_category_summary(out, "Stable", summary.stable_categories);
    write_category_summary(out, "Weakly stable", summary.weak_categories);

    out << "Matching-count distributions (matching_count:problem_count)\n"
        << "  stable: ";
    write_distribution(out, summary.stable_count_distribution);
    out << "\n  weakly stable: ";
    write_distribution(out, summary.weak_count_distribution);
    out << "\n\nBy directed colleague-rejection count K\n"
        << "K problems stable_exists_rate weakly_exists_rate simple_stable_rate "
           "doctor_stable_rate simple_weak_rate doctor_weak_rate\n";
    for (const auto &[k, stats] : summary.by_rejection_count) {
        out << k << ' ' << stats.problems << ' '
            << std::fixed << std::setprecision(6)
            << rate(stats.stable_exists, stats.problems) << ' '
            << rate(stats.weakly_stable_exists, stats.problems) << ' '
            << rate(stats.simple_stable, stats.problems) << ' '
            << rate(stats.doctor_stable, stats.problems) << ' '
            << rate(stats.simple_weakly_stable, stats.problems) << ' '
            << rate(stats.doctor_weakly_stable, stats.problems) << '\n';
    }
    out << "\nConsistency warnings\n"
        << "  warning_instances: " << summary.warning_instances << '\n'
        << "  enumerated stable=true and weakly_stable=false matchings: "
        << summary.stable_not_weakly_candidate_count << '\n'
        << "  simple stable=true and weakly_stable=false: "
        << summary.simple_stable_not_weakly << '\n'
        << "  doctor stable=true and weakly_stable=false: "
        << summary.doctor_stable_not_weakly << '\n'
        << "\nElapsed seconds: " << std::fixed << std::setprecision(6)
        << summary.elapsed_seconds << '\n';
}

void csv_quote(std::ostream &out, const std::string &value)
{
    out << '"';
    for (char c : value) {
        if (c == '"') out << '"';
        out << c;
    }
    out << '"';
}

template <typename Key>
std::string distribution_string(const std::map<Key, std::uint64_t> &distribution)
{
    std::ostringstream out;
    write_distribution(out, distribution);
    return out.str();
}

std::string k_stats_string(const Summary &summary)
{
    std::ostringstream out;
    bool first = true;
    for (const auto &[k, stats] : summary.by_rejection_count) {
        if (!first) out << ';';
        out << k << ':' << stats.problems << ':' << stats.stable_exists << ':'
            << stats.weakly_stable_exists << ':' << stats.simple_stable << ':'
            << stats.doctor_stable << ':' << stats.simple_weakly_stable << ':'
            << stats.doctor_weakly_stable;
        first = false;
    }
    return out.str();
}

void write_summary_csv(
    const std::filesystem::path &path,
    const CaseSpec &spec,
    std::uint64_t seed,
    const Summary &summary
)
{
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot open " + path.string());
    out << "case,mode,seed,n_firms,n_workers,capacities,problems,matching_candidates,"
        << "stable_exists,stable_exists_rate,stable_exists_false,weakly_stable_exists,"
        << "weakly_stable_exists_rate,weakly_stable_exists_false,"
        << "simple_stable,simple_stable_rate,doctor_stable,doctor_stable_rate,"
        << "simple_weakly_stable,simple_weakly_stable_rate,doctor_weakly_stable,"
        << "doctor_weakly_stable_rate,simple_conditional_stable_rate,"
        << "doctor_conditional_stable_rate,simple_conditional_weak_rate,"
        << "doctor_conditional_weak_rate,simple_missed_existing_stable,"
        << "doctor_missed_existing_stable,simple_missed_existing_weakly_stable,"
        << "doctor_missed_existing_weakly_stable,stable_both_success,stable_simple_only,"
        << "stable_doctor_only,stable_both_missed,stable_no_solution,weak_both_success,"
        << "weak_simple_only,weak_doctor_only,weak_both_missed,weak_no_solution,"
        << "warning_instances,stable_not_weakly_candidate_count,"
        << "simple_stable_not_weakly,doctor_stable_not_weakly,stable_count_distribution,"
        << "weak_count_distribution,k_stats,elapsed_seconds,firm_scores\n";
    out << spec.name << ',' << spec.mode << ',' << seed << ',' << spec.n_firms << ','
        << spec.n_workers << ',';
    std::ostringstream capacities;
    for (std::size_t i = 0; i < spec.capacities.size(); ++i) {
        if (i) capacities << ':';
        capacities << spec.capacities[i];
    }
    csv_quote(out, capacities.str());
    out << ',' << summary.total << ',' << spec.expected_matching_count << ','
        << summary.stable_exists << ',' << rate(summary.stable_exists, summary.total) << ','
        << summary.total - summary.stable_exists << ','
        << summary.weakly_stable_exists << ','
        << rate(summary.weakly_stable_exists, summary.total) << ','
        << summary.total - summary.weakly_stable_exists << ','
        << summary.simple_stable << ',' << rate(summary.simple_stable, summary.total) << ','
        << summary.doctor_stable << ',' << rate(summary.doctor_stable, summary.total) << ','
        << summary.simple_weakly_stable << ','
        << rate(summary.simple_weakly_stable, summary.total) << ','
        << summary.doctor_weakly_stable << ','
        << rate(summary.doctor_weakly_stable, summary.total) << ','
        << rate(summary.simple_stable, summary.stable_exists) << ','
        << rate(summary.doctor_stable, summary.stable_exists) << ','
        << rate(summary.simple_weakly_stable, summary.weakly_stable_exists) << ','
        << rate(summary.doctor_weakly_stable, summary.weakly_stable_exists) << ','
        << summary.stable_exists - summary.simple_stable << ','
        << summary.stable_exists - summary.doctor_stable << ','
        << summary.weakly_stable_exists - summary.simple_weakly_stable << ','
        << summary.weakly_stable_exists - summary.doctor_weakly_stable << ','
        << summary.stable_categories.both_success << ','
        << summary.stable_categories.simple_only << ','
        << summary.stable_categories.doctor_only << ','
        << summary.stable_categories.both_missed << ','
        << summary.stable_categories.no_solution << ','
        << summary.weak_categories.both_success << ','
        << summary.weak_categories.simple_only << ','
        << summary.weak_categories.doctor_only << ','
        << summary.weak_categories.both_missed << ','
        << summary.weak_categories.no_solution << ','
        << summary.warning_instances << ','
        << summary.stable_not_weakly_candidate_count << ','
        << summary.simple_stable_not_weakly << ','
        << summary.doctor_stable_not_weakly << ',';
    csv_quote(out, distribution_string(summary.stable_count_distribution));
    out << ',';
    csv_quote(out, distribution_string(summary.weak_count_distribution));
    out << ',';
    csv_quote(out, k_stats_string(summary));
    out << ',' << std::fixed << std::setprecision(6) << summary.elapsed_seconds << ','
        << spec.firm_scores << '\n';
}

} // namespace

CaseSpec case_spec(
    const std::string &case_name,
    std::uint64_t random_trials,
    const std::string &firm_scores
)
{
    if (!is_valid_firm_score_mode(firm_scores)) {
        throw std::invalid_argument("unknown firm score mode: " + firm_scores);
    }
    if (case_name == "2x3") {
        return {"2x3", "exhaustive", 2, 3, {2, 1}, 18432, 19, firm_scores};
    }
    if (case_name == "3x5") {
        if (random_trials == 0) throw std::invalid_argument("trials must be positive");
        return {"3x5", "random", 3, 5, {2, 2, 1}, random_trials, 456, firm_scores};
    }
    throw std::invalid_argument("unknown case: " + case_name);
}

bool is_valid_firm_score_mode(const std::string &firm_scores)
{
    return firm_scores == "linear" || firm_scores == "superincreasing";
}

void apply_firm_score_mode(Instance &instance, const std::string &firm_scores)
{
    if (firm_scores == "linear") return;
    if (firm_scores != "superincreasing") {
        throw std::invalid_argument("unknown firm score mode: " + firm_scores);
    }
    for (auto &row : instance.firm_prefs) {
        for (int &score : row) {
            if (score < 1 || score > 30) {
                throw std::logic_error("firm rank score out of range for superincreasing");
            }
            score = 1 << (score - 1);
        }
    }
}

std::vector<std::vector<int>> enumerate_assignment_vectors(
    int n_workers,
    const std::vector<int> &capacities
)
{
    if (n_workers < 0 || capacities.empty()) {
        throw std::invalid_argument("invalid dimensions for matching enumeration");
    }
    std::vector<int> used(capacities.size(), 0);
    std::vector<int> current(n_workers, UNMATCHED);
    std::vector<std::vector<int>> result;
    enumerate_assignments_recursive(
        0, n_workers, capacities, used, current, result);
    return result;
}

Instance make_exhaustive_2x3_instance(std::uint64_t index)
{
    constexpr std::uint64_t total = 18432;
    if (index >= total) throw std::out_of_range("2x3 exhaustive index out of range");
    static const auto worker_permutations = permutations_of_scores(2);
    static const auto firm_permutations = permutations_of_scores(3);

    const std::uint64_t colleague_mask = index % 64;
    index /= 64;
    const std::uint64_t firm_profile_index = index % 36;
    const std::uint64_t worker_profile_index = index / 36;

    Instance instance;
    std::uint64_t worker_digits = worker_profile_index;
    for (int worker = 0; worker < 3; ++worker) {
        instance.worker_prefs.push_back(
            worker_permutations[worker_digits % worker_permutations.size()]);
        worker_digits /= worker_permutations.size();
    }

    std::uint64_t firm_digits = firm_profile_index;
    for (int firm = 0; firm < 2; ++firm) {
        instance.firm_prefs.push_back(
            firm_permutations[firm_digits % firm_permutations.size()]);
        firm_digits /= firm_permutations.size();
    }

    instance.colleague_prefs.assign(3, std::vector<int>(3, 0));
    int bit = 0;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (i == j) continue;
            if ((colleague_mask >> bit) & 1ULL) {
                instance.colleague_prefs[i][j] = -INF;
            }
            ++bit;
        }
    }
    return instance;
}

Instance make_random_3x5_instance(std::uint64_t seed, std::uint64_t trial)
{
    const std::uint64_t trial_seed = splitmix64(seed ^ splitmix64(trial));
    std::mt19937_64 rng(trial_seed);
    Instance instance;
    for (int worker = 0; worker < 5; ++worker) {
        instance.worker_prefs.push_back(random_strict_scores(3, rng));
    }
    for (int firm = 0; firm < 3; ++firm) {
        instance.firm_prefs.push_back(random_strict_scores(5, rng));
    }
    instance.colleague_prefs.assign(5, std::vector<int>(5, 0));
    std::bernoulli_distribution rejects(0.5);
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            if (i != j && rejects(rng)) instance.colleague_prefs[i][j] = -INF;
        }
    }
    return instance;
}

std::uint32_t doctor_priority_seed(
    std::uint64_t seed,
    const std::string &case_name,
    std::uint64_t instance_index
)
{
    const std::uint64_t case_tag = case_name == "2x3"
        ? 0x23a2d4b86967f12bULL
        : 0x35e91b47c21a84d3ULL;
    return static_cast<std::uint32_t>(
        splitmix64(seed ^ case_tag ^ splitmix64(instance_index)));
}

std::string instance_fingerprint(const Instance &instance)
{
    std::ostringstream out;
    json_matrix(out, instance.worker_prefs);
    out << '|';
    json_matrix(out, instance.firm_prefs);
    out << '|';
    json_matrix(out, instance.colleague_prefs);
    return out.str();
}

std::filesystem::path create_run_directory(
    const std::filesystem::path &output_root,
    std::uint64_t seed,
    const std::string &firm_scores
)
{
    std::error_code error;
    std::filesystem::create_directories(output_root, error);
    if (error) {
        throw std::runtime_error(
            "cannot create output root " + output_root.string() + ": " + error.message());
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream base;
    base << "run_" << std::put_time(&local, "%Y%m%d_%H%M%S") << "_seed" << seed;
    if (firm_scores != "linear") base << "_firm" << firm_scores;
    for (int suffix = 0; ; ++suffix) {
        std::filesystem::path candidate = output_root / (
            base.str() + (suffix == 0 ? "" : "_" + std::to_string(suffix)));
        if (std::filesystem::create_directory(candidate, error)) return candidate;
        if (error) {
            throw std::runtime_error(
                "cannot create run directory " + candidate.string() + ": " + error.message());
        }
    }
}

void run_case(
    const CaseSpec &spec,
    std::uint64_t seed,
    const std::filesystem::path &case_output_directory
)
{
    const auto assignments = enumerate_assignment_vectors(spec.n_workers, spec.capacities);
    if (assignments.size() != spec.expected_matching_count) {
        throw std::logic_error(
            spec.name + " matching candidate count was "
            + std::to_string(assignments.size()) + ", expected "
            + std::to_string(spec.expected_matching_count));
    }
    std::filesystem::create_directories(case_output_directory);
    const auto instances_path = case_output_directory / "instances.jsonl";
    const auto interesting_path = case_output_directory / "interesting_cases.jsonl";
    std::ofstream instances_out(instances_path);
    std::ofstream interesting_out(interesting_path);
    if (!instances_out || !interesting_out) {
        throw std::runtime_error("cannot open JSONL output files");
    }

    Summary summary;
    const auto started = std::chrono::steady_clock::now();
    for (std::uint64_t index = 0; index < spec.problem_count; ++index) {
        Instance instance = spec.mode == "exhaustive"
            ? make_exhaustive_2x3_instance(index)
            : make_random_3x5_instance(seed, index);
        // Applied after generation so each index keeps the same ordinal profile
        // as the linear run, allowing paired comparison across score modes.
        apply_firm_score_mode(instance, spec.firm_scores);

        MatchingSystem system(spec.n_workers, spec.n_firms);
        system.set_prefs(
            instance.worker_prefs,
            instance.firm_prefs,
            instance.colleague_prefs
        );
        system.set_firm_capacities(spec.capacities);

        const std::uint32_t priority_seed = doctor_priority_seed(seed, spec.name, index);
        const AlgorithmResult simple = evaluate_algorithm(
            system.run_algorithm("simple_match"), instance, spec);
        const AlgorithmResult doctor = evaluate_algorithm(
            system.run_algorithm("doctor_dictator", priority_seed), instance, spec);
        const SearchResult search = exhaustive_search(instance, spec, assignments);

        if (search.checked_matching_count != spec.expected_matching_count
            || (search.stable_matching_count > 0) != search.stable_witness.has_value()
            || (search.weakly_stable_matching_count > 0)
                != search.weakly_stable_witness.has_value()) {
            throw std::logic_error("per-instance exhaustive-search consistency failure");
        }

        const int rejections = rejection_count(instance);
        const auto reasons = interesting_reasons(simple, doctor, search);
        const bool warning_instance =
            (simple.is_stable && !simple.is_weakly_stable)
            || (doctor.is_stable && !doctor.is_weakly_stable)
            || search.stable_not_weakly_matching_count > 0
            || (search.stable_matching_count > 0
                && search.weakly_stable_matching_count == 0);
        update_summary(
            summary, rejections, simple, doctor, search, warning_instance);
        write_instance_json(
            instances_out,
            spec,
            seed,
            index,
            priority_seed,
            instance,
            rejections,
            simple,
            doctor,
            search,
            nullptr
        );
        if (!reasons.empty()) {
            write_instance_json(
                interesting_out,
                spec,
                seed,
                index,
                priority_seed,
                instance,
                rejections,
                simple,
                doctor,
                search,
                &reasons
            );
        }

        if ((index + 1) % 1000 == 0 || index + 1 == spec.problem_count) {
            std::cerr << '[' << spec.name << "] " << index + 1 << '/'
                << spec.problem_count << " instances\n";
        }
    }
    summary.elapsed_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    validate_summary(summary);
    write_summary_txt(case_output_directory / "summary.txt", spec, seed, summary);
    write_summary_csv(case_output_directory / "summary.csv", spec, seed, summary);
}

} // namespace stability_simulation
