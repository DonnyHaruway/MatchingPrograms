"""Independent tiny-market model and end-to-end checks for the new CLI."""

import csv
import fcntl
import importlib.util
import itertools
import json
from pathlib import Path
import random
import signal
import subprocess
import sys
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "three_stabilities", ROOT / "experiments/universal_replacement/run_rda_stability_exhaustive.py")
DRIVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DRIVER)
BINARY = None


def read_csv(directory, name):
    with (directory / name).open(newline="") as stream:
        return list(csv.DictReader(stream))


def flags(problem, assignment):
    """Independent reference for the existing additive, binary-colleague definitions."""
    n, m = len(problem["capacities"]), len(assignment)
    ap, fp, cp = (problem[key] for key in ("agent_prefs", "firm_prefs", "colleague_prefs"))
    teams = [[w for w, f in enumerate(assignment) if f == firm] for firm in range(n)]
    def compatible(team):
        return all(cp[a][b] != -DRIVER.INF and cp[b][a] != -DRIVER.INF
                   for a, b in itertools.combinations(team, 2))
    if any(len(team) > problem["capacities"][f] or not compatible(team)
           for f, team in enumerate(teams)):
        return [False] * 3
    current = [0 if f == -1 else ap[w][f] for w, f in enumerate(assignment)]
    scores = [sum(fp[f][w] for w in team) for f, team in enumerate(teams)]
    if min(current) < 0 or min(scores) < 0:
        return [False] * 3
    answer = [True] * 3
    for firm, team in enumerate(teams):
        capacity = problem["capacities"][firm]
        for worker in range(m):
            if assignment[worker] == firm or ap[worker][firm] <= current[worker]:
                continue
            if len(team) < capacity:
                if fp[firm][worker] > 0 and compatible(team + [worker]):
                    answer[0] = answer[1] = False
            elif team:
                worst = min(team, key=lambda w: (fp[firm][w], w))
                if fp[firm][worker] > fp[firm][worst]:
                    if compatible([w for w in team if w != worst] + [worker]):
                        answer[1] = False
                    if all(compatible([w for w in team if w != removed] + [worker])
                           for removed in team):
                        answer[0] = False
            for size in range(min(capacity, len(team) + 1)):
                for retained in itertools.combinations(team, size):
                    after = list(retained) + [worker]
                    if compatible(after) and sum(fp[firm][w] for w in after) > scores[firm]:
                        answer[2] = False
    return answer


def reference_da(problem, current_firms):
    n, m = len(problem["capacities"]), len(problem["agent_prefs"])
    assignment = [-1] * m
    tried = [set() for _ in range(n)]
    while True:
        changed = False
        for f in range(n):
            if assignment.count(f) >= problem["capacities"][f]:
                continue
            candidates = [w for w in range(m) if w not in tried[f] and current_firms[f][w] > 0]
            if not candidates:
                continue
            w = max(candidates, key=lambda worker: current_firms[f][worker])
            tried[f].add(w)
            changed = True
            old_score = 0 if assignment[w] == -1 else problem["agent_prefs"][w][assignment[w]]
            if problem["agent_prefs"][w][f] > old_score:
                assignment[w] = f
        if not changed:
            return assignment


def reference(problem):
    n, m = len(problem["capacities"]), len(problem["agent_prefs"])
    leaves, nodes, max_depth = [], 0, 0
    current = [row[:] for row in problem["firm_prefs"]]
    def visit(depth):
        nonlocal nodes, max_depth
        nodes += 1
        max_depth = max(max_depth, depth)
        assignment = reference_da(problem, current)
        conflicts = [(f, a, b) for f in range(n)
                     for a, b in itertools.combinations(
                         [w for w in range(m) if assignment[w] == f], 2)
                     if problem["colleague_prefs"][a][b] == -DRIVER.INF]
        if not conflicts:
            leaves.append(tuple(assignment))
        for f, a, b in conflicts:
            for removed in (a, b):
                previous = current[f][removed]
                current[f][removed] = -DRIVER.INF
                visit(depth + 1)
                current[f][removed] = previous
    visit(0)
    existence_counts = [0] * 3
    candidates = 0
    for assignment in itertools.product(range(-1, n), repeat=m):
        if any(assignment.count(f) > problem["capacities"][f] for f in range(n)):
            continue
        candidates += 1
        for d, stable in enumerate(flags(problem, assignment)):
            existence_counts[d] += stable
    patterns, distinct_patterns = [0] * 8, [0] * 8
    for assignment in leaves:
        patterns[sum(int(v) << d for d, v in enumerate(flags(problem, assignment)))] += 1
    for assignment in set(leaves):
        distinct_patterns[sum(int(v) << d for d, v in enumerate(flags(problem, assignment)))] += 1
    return dict(nodes=nodes, branches=nodes - 1, leaves=len(leaves),
                distinct_matchings=len(set(leaves)), max_deletions=max_depth,
                stable_solution_counts=existence_counts, assignment_candidates=candidates,
                leaf_patterns=patterns, distinct_patterns=distinct_patterns)


class ThreeStabilities(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rda-three-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def command(self, *args, success=True):
        result = subprocess.run([str(BINARY), *map(str, args)], capture_output=True, text=True, timeout=60)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def run_market(self, name, *extra):
        output = self.root / name
        self.command("--firms", 2, "--workers", 2, "--capacities", "1,2",
                     "--forbidden-pairs", 1, "--output", output, *extra)
        return output

    def test_exact_counts_and_input_validation(self):
        for n, m, caps, counts in (
                (2, 2, "1,2", [648]), (2, 3, "1,2", [62208, 62208]),
                (3, 4, "1,1,2", [71663616000, 179159040000, 238878720000]),
                (3, 5, "1,2,2", [123834728448000, 557256278016000,
                                1486016741376000, 2600529297408000])):
            for k, expected in enumerate(counts, 1):
                result = self.command("--firms", n, "--workers", m, "--capacities", caps,
                                      "--forbidden-pairs", k, "--dry-run")
                self.assertEqual(json.loads(result.stdout)["total_representative_inputs"], expected)
        for extra in (["--count", 0], ["--start-id", -1], ["--count", 649],
                      ["--forbidden-pairs", 2], ["--capacities", "1"],
                      ["--progress-every", 0]):
            output = self.root / "invalid"
            self.command("--firms", 2, "--workers", 2, "--capacities", "1,2",
                         "--forbidden-pairs", 1, "--output", output, *extra, success=False)
            self.assertFalse(output.exists())

    def test_symmetry_representatives_cover_all_2x2_inputs(self):
        def key(problem):
            return tuple(tuple(row) for row in problem["worker_orders"] + problem["firm_orders"])
        def relabeled_key(workers, firms, permutation):
            after = [None, None]
            for old, new in enumerate(permutation):
                after[new - 1] = workers[old]
            return tuple(after) + tuple(tuple(0 if w == 0 else permutation[w - 1] for w in row)
                                        for row in firms)
        orders = list(itertools.permutations(range(3)))
        all_orbits = {min(relabeled_key(rows[:2], rows[2:], p)
                          for p in itertools.permutations((1, 2)))
                      for rows in itertools.product(orders, repeat=4)}
        domain = DRIVER.Domain(2, 2, [1, 2], 1)
        representative_orbits = set()
        for i in range(domain.total):
            problem = domain.problem(i)
            self.assertEqual([w for w in problem["firm_orders"][domain.anchor] if w], [1, 2])
            representative_orbits.add(min(
                relabeled_key(tuple(map(tuple, problem["worker_orders"])),
                              problem["firm_orders"], p)
                for p in itertools.permutations((1, 2))))
        self.assertEqual(len(all_orbits), 648)
        self.assertEqual(representative_orbits, all_orbits)

    def test_existing_engine_matches_independent_model(self):
        rng = random.Random(20261116)
        for n, m, capacities, forbidden in (
                (2, 2, [1, 2], 1), (2, 3, [1, 2], 1), (2, 3, [1, 2], 2),
                (3, 4, [1, 1, 2], 2), (3, 5, [1, 2, 2], 4)):
            domain = DRIVER.Domain(n, m, capacities, forbidden)
            engine = DRIVER.Engine(BINARY.with_name("rda_stability_engine"), domain)
            try:
                ids = list(range(domain.total)) if m == 2 else [
                    0, domain.total - 1, *[rng.randrange(domain.total) for _ in range(40)]]
                for i in ids:
                    problem = domain.problem(i)
                    actual, expected = engine.evaluate(problem), reference(problem)
                    for field, value in expected.items():
                        self.assertEqual(actual[field], value, (n, m, i, field))
                    for witness in actual["existence_witnesses"]:
                        if witness is not None:
                            assignment = [f - 1 for f in witness["assignment"]]
                            self.assertEqual(witness["stability"], flags(problem, assignment))
                    for leaf in actual["leaf_witnesses"]:
                        witness = leaf["matching"]
                        assignment = [f - 1 for f in witness["assignment"]]
                        self.assertEqual(witness["stability"], flags(problem, assignment))
                        current = [row[:] for row in problem["firm_prefs"]]
                        for step in leaf["path"]:
                            before = reference_da(problem, current)
                            self.assertEqual(step["parent_assignment"], [f + 1 for f in before])
                            f = step["firm"] - 1
                            a, b = (w - 1 for w in step["forbidden_pair"])
                            self.assertEqual(before[a], f)
                            self.assertEqual(before[b], f)
                            self.assertEqual(problem["colleague_prefs"][a][b], -DRIVER.INF)
                            current[f][step["removed_worker"] - 1] = -DRIVER.INF
                            self.assertEqual(step["child_assignment"],
                                             [f + 1 for f in reference_da(problem, current)])
                        self.assertEqual(assignment, reference_da(problem, current))
            finally:
                engine.close()

    def test_outputs_and_resume_are_identical_after_uncommitted_tail(self):
        whole = self.run_market("whole", "--write-profiles")
        resumed = self.run_market("resumed", "--write-profiles", "--stop-after", 301,
                                  "--progress-every", 100)
        self.assertFalse((resumed / "completion.json").exists())
        # Simulate writes after the last committed checkpoint and stale output.
        with (resumed / "profiles.csv").open("a") as stream:
            stream.write("uncommitted,garbage\n")
        (resumed / "summary.csv").write_text("stale summary")
        self.command("--resume", resumed)
        for name in ("summary.csv", "profile_classes.csv", "leaf_patterns.csv", "profiles.csv", "examples.jsonl"):
            self.assertEqual((whole / name).read_bytes(), (resumed / name).read_bytes(), name)
        completion = json.loads((resumed / "completion.json").read_text())
        self.assertTrue(completion["complete"])
        self.assertTrue(completion["whole_domain_complete"])
        self.assertEqual(completion["processed_representative_inputs"], 648)
        self.assertEqual(completion["processed_labeled_inputs"], 1296)
        summary = read_csv(whole, "summary.csv")
        self.assertEqual(len(summary), 6)
        for row in summary:
            self.assertEqual(int(row["inputs"]), int(row["stable_exists"]) + int(row["stable_not_exists"]))
            self.assertEqual(int(row["inputs"]), sum(int(row[name]) for name in
                                                     ("rda_all_stable", "rda_mixed", "rda_none_stable")))
        profiles = read_csv(whole, "profiles.csv")
        self.assertEqual(len(profiles), 648)
        self.assertEqual([int(row["instance_id"]) for row in profiles], list(range(648)))
        classes = read_csv(whole, "profile_classes.csv")
        self.assertEqual(sum(int(row["representative_inputs"]) for row in classes), 648)
        for item in map(json.loads, (whole / "examples.jsonl").read_text().splitlines()):
            self.assertTrue(item["existence_search_complete"])
        self.command("--resume", resumed, success=False)
        self.command("--firms", 2, "--workers", 2, "--capacities", "1,2",
                     "--forbidden-pairs", 1, "--output", whole, success=False)

    def test_partial_range_and_resume_reject_changed_config_or_checkpoint(self):
        partial = self.run_market("partial", "--start-id", 400, "--count", 10)
        self.assertFalse(json.loads((partial / "completion.json").read_text())["whole_domain_complete"])
        paused = self.run_market("paused", "--stop-after", 5)
        original = (paused / "config.json").read_bytes()
        data = json.loads(original)
        data["forbidden_pairs"] = 2
        (paused / "config.json").write_text(json.dumps(data))
        self.command("--resume", paused, success=False)
        (paused / "config.json").write_bytes(original)
        self.command("--resume", paused, "--capacities", "2,1", success=False)
        data = json.loads((paused / "checkpoint.json").read_text())
        data["payload"]["state"]["next_id"] += 1
        (paused / "checkpoint.json").write_text(json.dumps(data))
        self.command("--resume", paused, success=False)

    def test_signal_checkpoint_resume_and_exclusive_lock(self):
        output = self.root / "interrupted"
        process = subprocess.Popen(
            [str(BINARY), "--firms", "3", "--workers", "4", "--capacities", "1,1,2",
             "--forbidden-pairs", "2", "--count", "100000", "--output", str(output),
             "--progress-every", "10", "--write-profiles"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                progress = output / "progress.json"
                if progress.exists() and json.loads(progress.read_text())["processed_representative_inputs"] >= 10:
                    break
                time.sleep(0.01)
            else:
                self.fail("runner did not publish progress")
            process.send_signal(signal.SIGTERM)
            stdout, stderr = process.communicate(timeout=30)
            self.assertEqual(process.returncode, 128 + signal.SIGTERM, stderr)
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()
        self.assertFalse((output / "completion.json").exists())
        before = json.loads((output / "progress.json").read_text())["processed_representative_inputs"]
        with (output / ".run.lock").open("a") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            rejected = self.command("--resume", output, success=False)
            self.assertIn("another process", rejected.stderr)
        self.command("--resume", output, "--stop-after", 10)
        profiles = read_csv(output, "profiles.csv")
        self.assertEqual(len(profiles), before + 10)
        self.assertEqual([int(row["instance_id"]) for row in profiles], list(range(before + 10)))


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
