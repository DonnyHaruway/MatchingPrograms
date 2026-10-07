"""Independently verify a completed exhaustive_da_paths_5x8 output directory."""

import csv
import itertools
import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(directory):
    problem = json.loads((directory / "problem.json").read_text())
    wp = problem["agent_prefs"]
    fp = problem["firm_prefs"]
    cp = problem["colleague_prefs"]
    capacities = problem["capacities"]
    workers, firms = problem["n_workers"], problem["n_firms"]

    def feasible(members):
        return all(cp[a][b] != -100000 and cp[b][a] != -100000
                   for a, b in itertools.combinations(members, 2))

    def blocking_pairs(assignment):
        members = [[w for w, label in enumerate(assignment) if label == f + 1]
                   for f in range(firms)]
        require(len(assignment) == workers, "assignment length")
        require(all(0 <= label <= firms for label in assignment), "firm label")
        for f, team in enumerate(members):
            require(len(team) <= capacities[f] and feasible(team), "final feasibility")
            require(all(wp[w][f] >= 0 and fp[f][w] >= 0 for w in team),
                    "final individual rationality")
        blockers = []
        for f, team in enumerate(members):
            for w in range(workers):
                current = 0 if assignment[w] == 0 else wp[w][assignment[w] - 1]
                if wp[w][f] <= current or len(team) != capacities[f]:
                    continue
                if not any(fp[f][w] > fp[f][incumbent] for incumbent in team):
                    continue
                if all(feasible([other for other in team if other != removed] + [w])
                       for removed in team):
                    blockers.append({"firm": f + 1, "worker": w + 1})
        return blockers

    results = {}
    for algorithm in ("firm", "worker"):
        paths = [json.loads(line) for line in
                 (directory / f"{algorithm}_paths.jsonl").read_text().splitlines()]
        with (directory / f"{algorithm}_paths.csv").open() as source:
            rows = list(csv.DictReader(source))
        require(len(rows) == len(paths), "CSV / JSONL row count")
        nodes, leaves, outcomes = {}, set(), set()
        stable_count = 0
        for expected_id, (path, row) in enumerate(zip(paths, rows), 1):
            require(path["path_id"] == expected_id == int(row["path_id"]), "path ID")
            require(path["algorithm"] == algorithm, "algorithm name")
            assignment = path["assignment"]
            blockers = blocking_pairs(assignment)
            require(sorted(blockers, key=lambda p: (p["firm"], p["worker"])) ==
                    sorted(path["blocking_pairs"], key=lambda p: (p["firm"], p["worker"])),
                    "independent blocking-pair check")
            require(path["stable"] == (not blockers), "independent stability check")
            require(row["stable"] == str(path["stable"]).lower(), "CSV stability")
            require(path["matched_workers"] == sum(label != 0 for label in assignment)
                    == int(row["matched_workers"]), "matched count")
            require(assignment == [0 if row[f"w{w+1}"] == "unmatched"
                                   else int(row[f"w{w+1}"][1:]) for w in range(workers)],
                    "CSV assignment")
            expected_blockers = ";".join(f'f{p["firm"]}:w{p["worker"]}' for p in blockers)
            require(row["blocking_pairs"] == expected_blockers, "CSV blockers")
            stable_count += path["stable"]
            outcomes.add(tuple(assignment))
            prefix, deleted, deletion_text = (), set(), []
            last_round = 0
            for decision in path["decisions"]:
                before = decision["assignment_before"]
                require(decision["round"] >= last_round, "nonmonotone rounds")
                last_round = decision["round"]
                require(all(before[w - 1] != f for f, w in deleted),
                        "permanently deleted worker assigned again")
                description = {k: v for k, v in decision.items()
                               if k not in ("selected_index", "removed_worker", "firm")}
                node = nodes.setdefault(prefix, {"description": description, "choices": set()})
                require(node["description"] == description, "non-deterministic replay prefix")
                selected = decision["selected_index"]
                require(0 <= selected < decision["option_count"], "choice range")
                node["choices"].add(selected)
                prefix += (selected,)
                for pair in decision["pairs"]:
                    a, b, f = pair["left"] - 1, pair["right"] - 1, pair["firm"]
                    if decision["kind"] == "removal_endpoint":
                        require(before[a] == f == before[b], "unresolved removal pair")
                    require(not feasible([a, b]), "non-conflicting decision pair")
                if decision["kind"] == "removal_endpoint":
                    require(decision["option_count"] == 2 and len(decision["pairs"]) == 1,
                            "endpoint options")
                    pair = decision["pairs"][0]
                    removed = pair["left"] if selected == 0 else pair["right"]
                    require(removed == decision["removed_worker"] and
                            pair["firm"] == decision["firm"], "removed endpoint")
                    deleted.add((pair["firm"], removed))
                    deletion_text.append(f'r{decision["round"]}:f{pair["firm"]}:w{removed}')
                else:
                    require(decision["kind"] == "pair_order" and
                            decision["option_count"] == len(decision["pairs"]), "pair order")
            require(row["deletions"] == ";".join(deletion_text), "CSV deletions")
            require(all(assignment[w - 1] != f for f, w in deleted), "final deletion violation")
            require(prefix not in leaves, "duplicate path")
            leaves.add(prefix)
        require(not (leaves & nodes.keys()), "terminal path extended")
        for node in nodes.values():
            require(node["choices"] == set(range(node["description"]["option_count"])),
                    "missing decision branch")
        results[algorithm] = {"paths": len(paths), "stable_paths": stable_count,
                              "unstable_paths": len(paths) - stable_count,
                              "distinct_matchings": len(outcomes),
                              "all_decision_branches_verified": True}
    return results


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 verify_da_paths.py OUTPUT_DIRECTORY")
    print(json.dumps(verify(Path(sys.argv[1])), indent=2))
