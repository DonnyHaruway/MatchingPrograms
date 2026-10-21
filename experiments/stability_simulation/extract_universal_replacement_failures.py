#!/usr/bin/env python3
"""Extract saved profiles whose algorithm output fails universal replacement stability."""

import argparse
import csv
import json
import re
from contextlib import ExitStack
from pathlib import Path


def extract(source: Path, output_root: Path, all_algorithms: bool = False) -> Path:
    directory = output_root
    suffix = 0
    while True:
        try:
            directory.mkdir(parents=True, exist_ok=False)
            break
        except FileExistsError:
            suffix += 1
            directory = output_root.with_name(f"{output_root.name}_{suffix}")

    counts = {}
    files = {}
    first_example = None
    total = 0
    unique_failures = 0
    seen_trials = set()
    with ExitStack() as stack:
        records = stack.enter_context(source.open(encoding="utf-8"))
        union = stack.enter_context((directory / "profiles.jsonl").open("w", encoding="utf-8"))
        index_file = stack.enter_context((directory / "index.csv").open("w", newline="", encoding="utf-8"))
        index = csv.writer(index_file)
        index.writerow(["trial", "seed", "conflict_pairs", "algorithm", "is_weakly_stable",
                        "initial_da_conflict_pairs", "agent_match"])
        for line in records:
            record = json.loads(line)
            trial = record["trial"]
            if trial in seen_trials:
                raise ValueError(f"duplicate trial: {trial}")
            seen_trials.add(trial)
            algorithms = record["algorithms"]
            if not counts:
                if not algorithms:
                    raise ValueError("profile has no algorithms")
                for name in algorithms:
                    if not re.fullmatch(r"[A-Za-z0-9_]+", name):
                        raise ValueError(f"invalid algorithm name: {name}")
                    counts[name] = 0
                    files[name] = stack.enter_context((directory / f"{name}.jsonl").open("w", encoding="utf-8"))
            if algorithms.keys() != counts.keys():
                raise ValueError("algorithm list differs between profiles")
            for name, result in algorithms.items():
                if type(result["is_universal_replacement_stable"]) is not bool:
                    raise ValueError(f"non-Boolean stability result for {name}, trial {trial}")
            total += 1
            failed = [name for name, result in algorithms.items()
                      if result["is_universal_replacement_stable"] is False]
            if not failed:
                continue
            if all_algorithms and len(failed) != len(algorithms):
                continue
            unique_failures += 1
            record["failed_algorithms"] = failed
            serialized = json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n"
            union.write(serialized)
            if first_example is None:
                first_example = record
            for name in failed:
                counts[name] += 1
                files[name].write(serialized)
                result = algorithms[name]
                index.writerow([trial, record["seed"], record["conflict_pairs"], name,
                                result["is_weakly_stable"], result["initial_da_conflict_pairs"],
                                json.dumps(result["agent_match"], separators=(",", ":"))])
        if total == 0:
            raise ValueError("input contains no profiles")

    with (directory / "summary.md").open("w", encoding="utf-8") as report:
        report.write("# Universal replacement stabilityを満たさなかったケース\n\n")
        report.write(f"入力：`{source.resolve()}`\n\n")
        condition = "全方式が" if all_algorithms else "少なくとも1方式が"
        report.write(f"全{total:,}件のうち、{condition}満たさなかった選好は **{unique_failures:,}件**。\n\n")
        report.write(f"空席への追加blocking：{record.get('vacancy_blocking', 'メタデータなし（旧判定）')}\n\n")
        report.write("| アルゴリズム | 件数 | 割合 | 抽出データ |\n| --- | ---: | ---: | --- |\n")
        for name, count in counts.items():
            report.write(f"| `{name}` | {count:,} | {100 * count / total:.2f}% | [{name}.jsonl]({name}.jsonl) |\n")
        report.write("\n[profiles.jsonl](profiles.jsonl) は選好ごとに1件、[index.csv](index.csv) は失敗した方式ごとに1行です。\n")
        report.write("各JSONLには元の3種類の選好・定員・全方式の出力・seedと、`failed_algorithms` を保存しています。\n")
        report.write("trialは0始まり。所属企業ID・個人IDは0始まりで、未マッチは-1です。\n")
        if first_example is not None:
            report.write(f"\n## 最初のケース：trial {first_example['trial']}\n\n")
            report.write("| 方式 | 個人ID順の所属企業 | universal replacement stable |\n| --- | --- | --- |\n")
            for name, result in first_example["algorithms"].items():
                report.write(f"| `{name}` | `{result['agent_match']}` | {result['is_universal_replacement_stable']} |\n")
            report.write("\n```json\n")
            report.write(json.dumps(first_example, ensure_ascii=False, indent=2))
            report.write("\n```\n")

    print(f"Extracted {unique_failures}/{total} profiles to {directory}")
    for name, count in counts.items():
        print(f"  {name}: {count}")
    return directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("instances", type=Path, help="Source instances.jsonl")
    parser.add_argument("--output", type=Path, help="Output directory (existing directories are preserved)")
    parser.add_argument("--all-algorithms", action="store_true", help="Select only profiles where every algorithm fails")
    args = parser.parse_args()
    if not args.instances.is_file():
        parser.error(f"input file not found: {args.instances}")
    name = "all_algorithms_failed" if args.all_algorithms else "universal_replacement_failures"
    extract(args.instances, args.output or args.instances.parent / name, args.all_algorithms)


if __name__ == "__main__":
    main()
