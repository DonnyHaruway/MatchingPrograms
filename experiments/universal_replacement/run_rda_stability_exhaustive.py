#!/usr/bin/env python3
"""Symmetry-reduced exhaustive firm-proposing RDA with three stability tests.

Called by run_rda_exhaustive when the new market arguments are present.
Only the Python standard library is required. Counters use arbitrary precision.
"""

import argparse
from collections import Counter
import csv
import fcntl
import hashlib
import io
import itertools
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import time


SCHEMA = "rda_three_stabilities_v1"
STABILITIES = ("ur", "weak", "stability")
CATEGORIES = ("none_stable", "mixed", "all_stable")
INF = 100000
STATS = ("nodes", "branches", "leaves", "distinct_matchings",
         "assignment_candidates", "evaluated_assignments")
SUMMARY_COUNTS = ("inputs", "stable_exists", "stable_not_exists", "rda_all_stable",
                  "rda_mixed", "rda_none_stable", "exists_but_unreached",
                  "stable_leaves", "distinct_stable_matchings", "stable_solution_counts")


def encode_json(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def atomic_text(path, text):
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as stream:
        stream.write(text)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def atomic_json(path, value):
    atomic_text(path, encode_json(value) + "\n")


def csv_text(header, rows):
    stream = io.StringIO(newline="")
    writer = csv.writer(stream, lineterminator="\n")
    if header is not None:
        writer.writerow(header)
    writer.writerows(rows)
    return stream.getvalue()


class Domain:
    """Generate canonical representatives directly, rather than scan labeled inputs.

    A uniquely capacitated firm's order fixes the worker labels. Worker 1's strict
    order fixes labels within every equal-capacity firm group. Full unacceptable
    tails and every outside-option position remain in the domain.
    """

    def __init__(self, n, m, capacities, forbidden):
        if n not in (2, 3) or not 2 <= m <= 5:
            raise ValueError("supported dimensions: 2 or 3 firms, 2 through 5 workers")
        if len(capacities) != n or any(c < 0 or c > m for c in capacities):
            raise ValueError("capacities must contain one integer in [0, workers] per firm")
        if not 1 <= forbidden <= m - 1:
            raise ValueError("forbidden-pairs must be between 1 and workers-1")
        multiplicities = Counter(capacities)
        unique = [f for f, c in enumerate(capacities) if multiplicities[c] == 1]
        if not unique:
            raise ValueError("this canonical enumerator requires a uniquely capacitated firm; "
                             "the four agreed markets are supported")
        self.n, self.m = n, m
        self.capacities, self.forbidden = list(capacities), forbidden
        self.anchor = unique[0]
        self.weight = math.factorial(m) * math.prod(math.factorial(v) for v in multiplicities.values())
        firm_groups = [[f + 1 for f, c in enumerate(capacities) if c == cap]
                       for cap in sorted(multiplicities)]
        orders = list(itertools.permutations(range(n + 1)))
        first_orders = [order for order in orders if all(
            [label for label in order if label in group] == group for group in firm_groups)]
        self.worker_options = [first_orders] + [orders] * (m - 1)
        all_firm_orders = list(itertools.permutations(range(m + 1)))
        anchored = [tuple(list(range(1, m + 1))[:i] + [0] + list(range(1, m + 1))[i:])
                    for i in range(m + 1)]
        self.firm_options = [anchored if f == self.anchor else all_firm_orders for f in range(n)]
        pairs = list(itertools.combinations(range(1, m + 1), 2))
        self.forbidden_sets = list(itertools.combinations(pairs, forbidden))
        self.total = math.prod(len(options) for options in self.worker_options + self.firm_options)
        self.total *= len(self.forbidden_sets)
        raw = math.factorial(n + 1) ** m * math.factorial(m + 1) ** n * len(self.forbidden_sets)
        assert self.total * self.weight == raw

    @staticmethod
    def scores(order, alternatives):
        outside = order.index(0)
        return [outside - order.index(label) for label in range(1, alternatives + 1)]

    def problem(self, instance_id):
        if not 0 <= instance_id < self.total:
            raise ValueError("input ID outside representative domain")
        value, pair_index = divmod(instance_id, len(self.forbidden_sets))
        choices = self.worker_options + self.firm_options
        rows = [None] * len(choices)
        for i in reversed(range(len(choices))):
            value, index = divmod(value, len(choices[i]))
            rows[i] = list(choices[i][index])
        assert value == 0
        workers, firms = rows[:self.m], rows[self.m:]
        forbidden = self.forbidden_sets[pair_index]
        colleagues = [[0] * self.m for _ in range(self.m)]
        for left, right in forbidden:
            colleagues[left - 1][right - 1] = colleagues[right - 1][left - 1] = -INF
        return dict(instance_id=instance_id, symmetry_weight=self.weight,
                    capacities=self.capacities, worker_orders=workers, firm_orders=firms,
                    forbidden_pairs=forbidden,
                    agent_prefs=[self.scores(row, self.n) for row in workers],
                    firm_prefs=[self.scores(row, self.m) for row in firms],
                    colleague_prefs=colleagues)

    def metadata(self):
        return dict(n_firms=self.n, n_workers=self.m, capacities=self.capacities,
                    forbidden_pairs=self.forbidden, anchor_firm=self.anchor + 1,
                    symmetry_weight=self.weight, total_representative_inputs=self.total,
                    total_labeled_inputs=self.total * self.weight)


class Engine:
    def __init__(self, executable, domain):
        self.process = subprocess.Popen(
            [str(executable), str(domain.n), str(domain.m), *map(str, domain.capacities)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1,
            start_new_session=True)

    def evaluate(self, problem):
        fields = [problem["instance_id"]]
        for key in ("agent_prefs", "firm_prefs", "colleague_prefs"):
            fields.extend(score for row in problem[key] for score in row)
        self.process.stdin.write(" ".join(map(str, fields)) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(f"C++ evaluation engine stopped (status {self.process.poll()})")
        result = json.loads(line)
        if result["instance_id"] != problem["instance_id"]:
            raise RuntimeError("engine returned an unexpected input ID")
        return result

    def close(self):
        self.process.stdin.close()
        self.process.stdout.close()
        try:
            code = self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=10)
            raise RuntimeError("evaluation engine did not stop")
        if code:
            raise RuntimeError(f"evaluation engine failed with status {code}")


def initial_state(start):
    return dict(next_id=start, processed=0, elapsed_seconds=0.0, profiles_offset=0,
                summaries=[dict.fromkeys(SUMMARY_COUNTS, 0) for _ in STABILITIES],
                classes={}, patterns={}, examples={},
                stats=dict.fromkeys(STATS, 0), max_nodes=0, max_deletions=0,
                rda_seconds=0.0, existence_seconds=0.0)


def classify(result):
    return [2 if count == result["leaves"] else 1 if count else 0
            for count in result["stable_leaves"]]


def add_result(state, problem, result):
    categories = classify(result)
    exists = [count > 0 for count in result["stable_solution_counts"]]
    if result["leaves"] < 1 or result["nodes"] != result["branches"] + 1:
        raise RuntimeError("invalid RDA tree accounting")
    if sum(result["leaf_patterns"]) != result["leaves"]:
        raise RuntimeError("invalid leaf pattern accounting")
    if sum(result["distinct_patterns"]) != result["distinct_matchings"]:
        raise RuntimeError("invalid distinct matching accounting")
    if any(result["distinct_stable_matchings"][d] > result["stable_solution_counts"][d]
           for d in range(3)):
        raise RuntimeError("RDA stable outputs exceed existing stable matchings")
    state["processed"] += 1
    state["next_id"] = problem["instance_id"] + 1
    for d, summary in enumerate(state["summaries"]):
        summary["inputs"] += 1
        summary["stable_exists"] += exists[d]
        summary["stable_not_exists"] += not exists[d]
        summary["rda_" + CATEGORIES[categories[d]]] += 1
        summary["exists_but_unreached"] += exists[d] and categories[d] == 0
        for metric in ("stable_leaves", "distinct_stable_matchings", "stable_solution_counts"):
            summary[metric] += result[metric][d]
    key = ":".join(map(str, [result["initial_conflicts"], *map(int, exists), *categories]))
    state["classes"][key] = state["classes"].get(key, 0) + 1
    if key not in state["examples"]:
        state["examples"][key] = dict(
            input=problem, result={k: v for k, v in result.items() if not k.endswith("_seconds")},
            existence_search_complete=True,
            categories=dict(zip(STABILITIES, map(CATEGORIES.__getitem__, categories))))
    for pattern, paths in enumerate(result["leaf_patterns"]):
        if not paths:
            continue
        key = f'{result["initial_conflicts"]}:{pattern}'
        totals = state["patterns"].setdefault(key, [0, 0, 0])
        totals[0] += 1
        totals[1] += paths
        totals[2] += result["distinct_patterns"][pattern]
    for metric in STATS:
        state["stats"][metric] += result[metric]
    state["max_nodes"] = max(state["max_nodes"], result["nodes"])
    state["max_deletions"] = max(state["max_deletions"], result["max_deletions"])
    state["rda_seconds"] += result["rda_seconds"]
    state["existence_seconds"] += result["existence_seconds"]


def profile_row(problem, result):
    row = [problem["instance_id"], problem["symmetry_weight"], result["initial_conflicts"]]
    for d in range(3):
        row += [int(result["stable_solution_counts"][d] > 0), CATEGORIES[classify(result)[d]],
                result["stable_leaves"][d], result["distinct_stable_matchings"][d],
                result["stable_solution_counts"][d]]
    return row + [result[metric] for metric in STATS] + [result["max_deletions"]]


PROFILE_HEADER = ["instance_id", "symmetry_weight", "initial_conflicts"]
for _name in STABILITIES:
    PROFILE_HEADER += [f"{_name}_{suffix}" for suffix in
                       ("exists", "category", "stable_leaves", "distinct_stable_matchings",
                        "stable_solution_count")]
PROFILE_HEADER += list(STATS) + ["max_deletions"]


def publish(output, config, state):
    weight = config["symmetry_weight"]
    rows = []
    for name, summary in zip(STABILITIES, state["summaries"]):
        for basis, multiplier in (("representative", 1), ("labeled", weight)):
            reached = summary["rda_all_stable"] + summary["rda_mixed"]
            rate = (format(reached / summary["stable_exists"], ".12g")
                    if summary["stable_exists"] else "")
            rows.append([name, basis, *[summary[key] * multiplier for key in SUMMARY_COUNTS], rate])
    atomic_text(output / "summary.csv", csv_text(
        ["stability", "count_basis", *SUMMARY_COUNTS, "reach_rate_given_exists"], rows))
    rows = []
    for key, count in sorted(state["classes"].items()):
        conflicts, *flags = map(int, key.split(":"))
        rows.append([conflicts, *flags[:3], *map(CATEGORIES.__getitem__, flags[3:]),
                     count, count * weight])
    atomic_text(output / "profile_classes.csv", csv_text(
        ["initial_conflicts", "ur_exists", "weak_exists", "stability_exists",
         "ur_category", "weak_category", "stability_category",
         "representative_inputs", "labeled_inputs"], rows))
    rows = []
    for key, totals in sorted(state["patterns"].items()):
        conflicts, pattern = map(int, key.split(":"))
        rows.append([conflicts, *[(pattern >> d) & 1 for d in range(3)],
                     totals[0], totals[0] * weight, totals[1], totals[1] * weight,
                     totals[2], totals[2] * weight])
    atomic_text(output / "leaf_patterns.csv", csv_text(
        ["initial_conflicts", "ur_stable", "weakly_stable", "stable",
         "representative_inputs_with_pattern", "labeled_inputs_with_pattern",
         "leaf_paths_representative", "leaf_paths_labeled",
         "distinct_matchings_representative", "distinct_matchings_labeled"], rows))
    rows = [[state["processed"], state["processed"] * weight,
             *[state["stats"][metric] for metric in STATS],
             state["max_nodes"], state["max_deletions"],
             format(state["elapsed_seconds"], ".9f"),
             format(state["rda_seconds"], ".9f"), format(state["existence_seconds"], ".9f")]]
    atomic_text(output / "search_stats.csv", csv_text(
        ["representative_inputs", "labeled_inputs", *STATS,
         "max_nodes", "max_deletions", "elapsed_seconds", "rda_seconds", "existence_seconds"], rows))
    examples = sorted(state["examples"].values(), key=lambda item: item["input"]["instance_id"])
    atomic_text(output / "examples.jsonl", "".join(encode_json(item) + "\n" for item in examples))


def save_checkpoint(output, config, state, profiles):
    if profiles:
        profiles.flush()
        os.fsync(profiles.fileno())
        state["profiles_offset"] = profiles.tell()
    data = encode_json(dict(config=config, state=state))
    atomic_json(output / "checkpoint.json",
                dict(sha256=hashlib.sha256(data.encode()).hexdigest(), payload=json.loads(data)))
    # checkpoint.json is authoritative. Other files can be regenerated after a
    # crash between publishing them; profiles.csv is rolled back to its offset.
    publish(output, config, state)
    atomic_json(output / "progress.json", dict(
        processed_representative_inputs=state["processed"],
        processed_labeled_inputs=state["processed"] * config["symmetry_weight"],
        selected_representative_inputs=config["end_id_exclusive"] - config["start_id"],
        next_id=state["next_id"], elapsed_seconds=state["elapsed_seconds"],
        checkpoint_sha256=hashlib.sha256(data.encode()).hexdigest()))


def load_checkpoint(output):
    checkpoint = json.loads((output / "checkpoint.json").read_text())
    data = encode_json(checkpoint["payload"])
    if hashlib.sha256(data.encode()).hexdigest() != checkpoint["sha256"]:
        raise ValueError("checkpoint checksum mismatch")
    config, state = checkpoint["payload"]["config"], checkpoint["payload"]["state"]
    if json.loads((output / "config.json").read_text()) != config:
        raise ValueError("config.json does not match checkpoint")
    if state["next_id"] != config["start_id"] + state["processed"]:
        raise ValueError("checkpoint progress is inconsistent")
    if not config["start_id"] <= state["next_id"] <= config["end_id_exclusive"]:
        raise ValueError("checkpoint input ID is outside the selected range")
    if any(row["inputs"] != state["processed"] for row in state["summaries"]):
        raise ValueError("checkpoint summaries do not match processed count")
    if sum(state["classes"].values()) != state["processed"]:
        raise ValueError("checkpoint classes do not match processed count")
    return config, state


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--engine", type=Path, required=True, help=argparse.SUPPRESS)
    p.add_argument("--firms", type=int)
    p.add_argument("--workers", type=int)
    p.add_argument("--capacities", help="comma-separated capacities in firm-label order")
    p.add_argument("--forbidden-pairs", type=int)
    p.add_argument("--output", type=Path, help="new directory for all results")
    p.add_argument("--resume", type=Path, help="resume an existing output directory")
    p.add_argument("--start-id", type=int, help="first representative ID, default 0")
    p.add_argument("--count", type=int, help="number of representatives, default all remaining")
    p.add_argument("--progress-every", type=int, default=1000)
    p.add_argument("--write-profiles", action="store_true", default=None)
    p.add_argument("--dry-run", action="store_true", help="print exact counts without writing files")
    p.add_argument("--stop-after", type=int, help="checkpoint and stop after N inputs in this invocation")
    return p


def new_config(args, engine_hash):
    if None in (args.firms, args.workers, args.capacities, args.forbidden_pairs):
        raise ValueError("--firms, --workers, --capacities and --forbidden-pairs are required")
    capacities = [int(c) for c in args.capacities.split(",")]
    domain = Domain(args.firms, args.workers, capacities, args.forbidden_pairs)
    start = args.start_id if args.start_id is not None else 0
    count = args.count if args.count is not None else domain.total - start
    if not 0 <= start < domain.total or count < 1 or count > domain.total - start:
        raise ValueError("requested representative range is outside the domain")
    config = dict(
        schema_version=SCHEMA, **domain.metadata(), start_id=start, end_id_exclusive=start + count,
        proposing_side="firm", stability_against_original_preferences=True,
        vacancy_blocking=True, full_path_enumeration=True, duplicate_branches_kept=True,
        early_stopping=False, full_assignment_enumeration=True,
        symmetric_forbidden_pairs=True, outside_score=0,
        score_rule="outside_rank_minus_counterpart_rank",
        order_labels="one_based_counterparts_zero_outside",
        enumeration="anchor_firm_worker_order_then_w1_equal_capacity_order_v1;"
                    "worker_rows_then_firm_rows_then_lexicographic_forbidden_sets_fastest",
        example_selection="minimum_representative_id_per_joint_class_and_initial_conflicts",
        write_profiles=bool(args.write_profiles), engine_sha256=engine_hash,
        driver_sha256=digest(__file__))
    return domain, config


def main(argv=None):
    args = parser().parse_args(argv)
    if args.progress_every < 1 or (args.stop_after is not None and args.stop_after < 1):
        raise ValueError("progress-every and stop-after must be positive")
    engine = args.engine.resolve()
    if not engine.is_file():
        raise ValueError("C++ engine not found; build the run_rda_exhaustive CMake target")
    engine_hash = digest(engine)
    if args.resume and args.dry_run:
        raise ValueError("--resume and --dry-run cannot be combined")
    if args.resume:
        if args.output is not None:
            raise ValueError("use --resume DIRECTORY without --output")
        output = args.resume.resolve()
        if not output.is_dir():
            raise ValueError("resume directory does not exist")
    else:
        domain, config = new_config(args, engine_hash)
        if args.dry_run:
            print(encode_json(dict(**domain.metadata(), start_id=config["start_id"],
                                  end_id_exclusive=config["end_id_exclusive"],
                                  selected_inputs=config["end_id_exclusive"] - config["start_id"])))
            return 0
        if args.output is None:
            raise ValueError("--output is required")
        output = args.output.resolve()
        # Refuse any existing directory, including a symlink, before writing.
        output.mkdir(parents=True, exist_ok=False)
    with (output / ".run.lock").open("a") as lock:
        try:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError("another process is already using this output directory")
        if args.resume:
            if (output / "completion.json").exists():
                raise ValueError("this selected range is already complete")
            config, state = load_checkpoint(output)
            if config["schema_version"] != SCHEMA:
                raise ValueError("unsupported checkpoint schema")
            if config["engine_sha256"] != engine_hash or config["driver_sha256"] != digest(__file__):
                raise ValueError("implementation changed since checkpoint; use the original build and driver")
            for key, supplied in (("n_firms", args.firms), ("n_workers", args.workers),
                                  ("forbidden_pairs", args.forbidden_pairs),
                                  ("start_id", args.start_id), ("write_profiles", args.write_profiles)):
                if supplied is not None and supplied != config[key]:
                    raise ValueError(f"resume option conflicts with saved {key}")
            if args.capacities is not None and [int(c) for c in args.capacities.split(",")] != config["capacities"]:
                raise ValueError("resume capacities conflict with saved configuration")
            if args.count is not None and args.count != config["end_id_exclusive"] - config["start_id"]:
                raise ValueError("resume count conflicts with saved range")
            domain = Domain(config["n_firms"], config["n_workers"], config["capacities"], config["forbidden_pairs"])
        else:
            state = initial_state(config["start_id"])
            atomic_json(output / "config.json", config)
        profiles = None
        evaluator = None
        old_handlers = {}
        interrupted = []
        def stop(signum, frame):
            interrupted.append(signum)
        try:
            if config["write_profiles"]:
                if args.resume:
                    profiles = (output / "profiles.csv").open("r+b")
                    profiles.seek(0, 2)
                    if profiles.tell() < state["profiles_offset"]:
                        raise ValueError("profiles.csv is shorter than its checkpoint offset")
                    profiles.truncate(state["profiles_offset"])
                    profiles.seek(state["profiles_offset"])
                else:
                    profiles = (output / "profiles.csv").open("w+b")
                    profiles.write(csv_text(PROFILE_HEADER, []).encode())
            base_elapsed, invocation_start = state["elapsed_seconds"], time.monotonic()
            save_checkpoint(output, config, state, profiles)
            for signum in (signal.SIGINT, signal.SIGTERM):
                old_handlers[signum] = signal.signal(signum, stop)
            evaluator = Engine(engine, domain)
            processed_this_time = 0
            while state["next_id"] < config["end_id_exclusive"]:
                if interrupted or (args.stop_after is not None and processed_this_time >= args.stop_after):
                    break
                problem = domain.problem(state["next_id"])
                result = evaluator.evaluate(problem)
                add_result(state, problem, result)
                if profiles:
                    profiles.write(csv_text(None, [profile_row(problem, result)]).encode())
                processed_this_time += 1
                if state["processed"] % args.progress_every == 0:
                    state["elapsed_seconds"] = base_elapsed + time.monotonic() - invocation_start
                    save_checkpoint(output, config, state, profiles)
                    print(f'processed={state["processed"]} next_id={state["next_id"]} '
                          f'elapsed_seconds={state["elapsed_seconds"]:.3f}', file=sys.stderr, flush=True)
            evaluator.close()
            evaluator = None
            state["elapsed_seconds"] = base_elapsed + time.monotonic() - invocation_start
            save_checkpoint(output, config, state, profiles)
            if state["next_id"] == config["end_id_exclusive"]:
                atomic_json(output / "completion.json", dict(
                    schema_version=SCHEMA, complete=True,
                    whole_domain_complete=config["start_id"] == 0 and state["next_id"] == domain.total,
                    processed_representative_inputs=state["processed"],
                    processed_labeled_inputs=state["processed"] * domain.weight,
                    next_id=state["next_id"], elapsed_seconds=state["elapsed_seconds"],
                    checkpoint_sha256=json.loads((output / "progress.json").read_text())["checkpoint_sha256"]))
            print(f'Outputs: {output}\nProcessed representatives: {state["processed"]}\n'
                  f'Complete selected range: {state["next_id"] == config["end_id_exclusive"]}')
            return 128 + interrupted[0] if interrupted else 0
        finally:
            for signum, handler in old_handlers.items():
                signal.signal(signum, handler)
            if profiles:
                profiles.close()
            if evaluator is not None:
                evaluator.process.terminate()
                evaluator.process.wait(timeout=10)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, RuntimeError, KeyError, json.JSONDecodeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
