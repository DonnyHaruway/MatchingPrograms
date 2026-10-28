"""Merge completed, non-overlapping input ranges using only the Python standard library."""

import argparse
import csv
import json
import math
from pathlib import Path


KEYS = ["scope", "forbidden_pairs", "initial_conflicts"]
SUMS = [
    "inputs", "all_stable", "mixed", "none_stable", "nodes", "branches", "leaves",
    "stable_leaves", "distinct_matchings", "distinct_stable_matchings",
]
MAXIMA = ["max_nodes", "max_deletions"]
FIELDS = KEYS + SUMS + MAXIMA
DOMAIN_FIELDS = [
    "schema_version", "experiment", "proposing_side", "n_workers", "n_firms", "capacities",
    "total_domain_inputs", "worker_order_count", "firm_order_count", "forbidden_masks",
    "order_labels", "enumeration", "outside_score", "score_rule", "symmetric_forbidden_pairs",
    "full_path_enumeration", "duplicate_branches_kept", "early_stopping", "vacancy_blocking",
    "stability_against_original_preferences",
]


def read_json(path):
    return json.loads(path.read_text())


def checked_ranges(config):
    intervals = config.get("ranges", [[config["start_id"], config["end_id_exclusive"]]])
    if not intervals or any(
        len(pair) != 2 or not all(type(x) is int for x in pair)
        or not 0 <= pair[0] < pair[1] <= config["total_domain_inputs"]
        for pair in intervals
    ):
        raise ValueError("invalid input ranges")
    intervals = sorted(intervals)
    if any(left[1] > right[0] for left, right in zip(intervals, intervals[1:])):
        raise ValueError("overlapping input ranges")
    if sum(end - start for start, end in intervals) != config["selected_inputs"]:
        raise ValueError("range lengths disagree with selected_inputs")
    if config["start_id"] != intervals[0][0] or config["end_id_exclusive"] != intervals[-1][1]:
        raise ValueError("range bounds disagree with config")
    return intervals


def combine(left, right):
    return {
        **{field: left.get(field, 0) + right[field] for field in SUMS},
        **{field: max(left.get(field, 0), right[field]) for field in MAXIMA},
    }


def read_summary(path, config):
    rows = {}
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != FIELDS:
            raise ValueError(f"unexpected summary columns: {path}")
        for row in reader:
            key = (row["scope"], int(row["forbidden_pairs"]), int(row["initial_conflicts"]))
            totals = {field: int(row[field]) for field in SUMS + MAXIMA}
            if key in rows or any(value < 0 for value in totals.values()):
                raise ValueError(f"duplicate row or negative counts: {path}")
            if key != ("overall", -1, -1) and not (
                key[0] == "by_conflicts_and_forbidden"
                and 0 <= key[1] <= math.comb(config["n_workers"], 2)
                and 0 <= key[2] <= sum(capacity // 2 for capacity in config["capacities"])
            ):
                raise ValueError(f"invalid summary group: {path}")
            if (
                totals["all_stable"] + totals["mixed"] + totals["none_stable"] != totals["inputs"]
                or totals["nodes"] != totals["branches"] + totals["inputs"]
                or not 0 <= totals["distinct_stable_matchings"] <= totals["stable_leaves"] <= totals["leaves"]
                or not 0 <= totals["distinct_stable_matchings"] <= totals["distinct_matchings"] <= totals["leaves"]
            ):
                raise ValueError(f"inconsistent summary counts: {path}")
            rows[key] = totals
    overall = rows.get(("overall", -1, -1))
    combined = {field: 0 for field in SUMS + MAXIMA}
    for key, totals in rows.items():
        if key[0] != "overall":
            combined = combine(combined, totals)
    if overall != combined or overall["inputs"] != config["selected_inputs"]:
        raise ValueError(f"summary totals disagree with groups/config: {path}")
    return rows


def merge(directories, output, require_full=False):
    if output.exists():
        raise ValueError(f"output directory already exists: {output}")
    baseline = None
    intervals, summaries, examples = [], {}, {}
    elapsed = 0.0
    for directory in directories:
        config = read_json(directory / "config.json")
        identity = {field: config[field] for field in DOMAIN_FIELDS}
        if baseline is None:
            baseline = identity
        elif identity != baseline:
            raise ValueError(f"different experiment domains: {directory}")
        selected_ranges = checked_ranges(config)
        completed = read_json(directory / "completion.json")  # Missing completion is an error.
        if (
            completed.get("complete") is not True
            or completed["processed_inputs"] != config["selected_inputs"]
            or completed["next_id"] != config["end_id_exclusive"]
        ):
            raise ValueError(f"run is incomplete: {directory}")
        rows = read_summary(directory / "summary.csv", config)
        if completed["nodes"] != rows[("overall", -1, -1)]["nodes"]:
            raise ValueError(f"completion disagrees with summary: {directory}")
        for key, totals in rows.items():
            summaries[key] = combine(summaries.get(key, {}), totals)
        intervals.extend(selected_ranges)
        elapsed += completed["elapsed_seconds"]
        with (directory / "examples.jsonl").open() as stream:
            for line in stream:
                example = json.loads(line)
                if not any(start <= example["instance_id"] < end for start, end in selected_ranges):
                    raise ValueError(f"example ID outside input ranges: {directory}")
                key = (len(example["forbidden_pairs"]), example["initial_conflicts"], example["category"])
                if key not in examples or example["instance_id"] < examples[key]["instance_id"]:
                    examples[key] = example
    intervals.sort()
    if any(left[1] > right[0] for left, right in zip(intervals, intervals[1:])):
        raise ValueError("input ranges overlap; refusing to double-count inputs")
    full = (
        intervals[0][0] == 0 and intervals[-1][1] == baseline["total_domain_inputs"]
        and all(left[1] == right[0] for left, right in zip(intervals, intervals[1:]))
    )
    if require_full and not full:
        raise ValueError("input ranges do not cover the entire domain without gaps")
    inputs = sum(end - start for start, end in intervals)
    config = {
        **baseline, "start_id": intervals[0][0], "end_id_exclusive": intervals[-1][1],
        "ranges": intervals, "selected_inputs": inputs, "write_profiles": False,
        "source_directories": [str(path.resolve()) for path in directories],
    }
    output.mkdir(parents=True)
    (output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    with (output / "summary.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        for key in sorted(summaries, key=lambda key: (key[0] != "overall", key)):
            writer.writerow({**dict(zip(KEYS, key)), **summaries[key]})
    with (output / "examples.jsonl").open("w") as stream:
        for key in sorted(examples):
            stream.write(json.dumps(examples[key], separators=(",", ":")) + "\n")
    completion = {
        "complete": True, "whole_domain_complete": full, "processed_inputs": inputs,
        "next_id": intervals[-1][1], "nodes": summaries[("overall", -1, -1)]["nodes"],
        "elapsed_seconds": elapsed, "elapsed_seconds_is_sum_of_source_runs": True,
    }
    (output / "completion.json").write_text(json.dumps(completion, indent=2) + "\n")
    print(f"Merged {inputs:,} inputs; whole_domain_complete={full}; output={output.resolve()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directories", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--require-full", action="store_true", help="Reject gaps or missing domain boundaries")
    args = parser.parse_args()
    try:
        merge(args.directories, args.output, args.require_full)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
