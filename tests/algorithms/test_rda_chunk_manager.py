"""Range coverage, scheduling, recovery and scientific-result merge checks."""

import csv
import fcntl
import json
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest


EXPERIMENT = Path(__file__).resolve().parents[2] / "experiments/universal_replacement"
sys.path.insert(0, str(EXPERIMENT))
import manage_rda_exhaustive as manager
import run_rda_stability_exhaustive as runner

BINARY = None
MANAGER = EXPERIMENT / "manage_rda_exhaustive.py"


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


class ChunkManager(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rda-manager-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def command(self, *args, success=True):
        result = subprocess.run(
            [sys.executable, str(MANAGER), *map(str, args)],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def create(self, name, command="plan", **options):
        output = self.root / name
        args = [command, "--binary", BINARY,
                "--firms", options.pop("firms", 2), "--workers", options.pop("workers", 2),
                "--capacities", options.pop("capacities", "1,2"),
                "--forbidden-pairs", options.pop("forbidden_pairs", 1), "--output", output]
        for key, value in options.items():
            if value is not False:
                args.append("--" + key.replace("_", "-"))
                if value is not True:
                    args.append(value)
        self.command(*args)
        return output

    def direct(self, output, workers=2, forbidden=1, start=0, count=None, profiles=False, stop=None):
        args = [str(BINARY), "--firms", "2", "--workers", str(workers),
                "--capacities", "1,2", "--forbidden-pairs", str(forbidden),
                "--start-id", str(start), "--output", str(output), "--progress-every", "100000"]
        if count is not None:
            args += ["--count", str(count)]
        if profiles:
            args.append("--write-profiles")
        if stop is not None:
            args += ["--stop-after", str(stop)]
        subprocess.run(args, check=True, capture_output=True, text=True, timeout=60)

    def compare_outputs(self, expected, actual, profiles=False):
        for name in ("summary.csv", "profile_classes.csv", "leaf_patterns.csv", "examples.jsonl"):
            self.assertEqual((expected / name).read_bytes(), (actual / name).read_bytes(), name)
        if profiles:
            self.assertEqual((expected / "profiles.csv").read_bytes(), (actual / "profiles.csv").read_bytes())
        left, right = rows(expected / "search_stats.csv")[0], rows(actual / "search_stats.csv")[0]
        for key in left:
            if not key.endswith("_seconds"):
                self.assertEqual(left[key], right[key], key)

    def test_plan_exact_coverage_partial_ranges_and_large_integer_ids(self):
        output = self.create("plan", chunk_size=100)
        plan = manager.Plan.load(output)
        manifest = rows(output / "chunks.csv")
        self.assertEqual(len(manifest), 7)
        self.assertEqual([int(row["count"]) for row in manifest], [100] * 6 + [48])
        self.assertEqual(sum(int(row["count"]) for row in manifest), 648)
        self.assertEqual(int(manifest[0]["start_id"]), 0)
        self.assertEqual(int(manifest[-1]["end_id_exclusive"]), 648)
        for left, right in zip(manifest, manifest[1:]):
            self.assertEqual(left["end_id_exclusive"], right["start_id"])
        self.assertFalse(any((output / "chunks").iterdir()))
        partial = self.create("partial", start_id=400, count=23, chunk_size=10)
        self.assertEqual([chunk["count"] for chunk in manager.Plan.load(partial).chunks()], [10, 10, 3])
        large = self.create("large", firms=3, workers=5, capacities="1,2,2",
                            forbidden_pairs=4, chunk_size=100000000000000)
        last = list(manager.Plan.load(large).chunks())[-1]
        self.assertEqual(last["end_id_exclusive"], 2600529297408000)
        self.assertEqual(manager.Plan.load(large).config["total_labeled_inputs"], 624127031377920000)
        self.assertNotIn("e+", (large / "chunks.csv").read_text())
        dry = self.root / "dry"
        result = self.command("plan", "--binary", BINARY, "--firms", 3, "--workers", 4,
                              "--capacities", "1,1,2", "--forbidden-pairs", 1,
                              "--output", dry, "--dry-run")
        self.assertEqual(json.loads(result.stdout)["chunk_count"], 717)
        self.assertFalse(dry.exists())
        self.assertEqual(plan.config["symmetry_weight"], 2)

    def test_full_parallel_merges_equal_direct_runs_and_are_idempotent(self):
        for workers, forbidden, size, profiles, jobs in [
                (2, 1, 137, True, 3), (3, 2, 10000, False, 3), (2, 1, 20, False, 32)]:
            name = f"{workers}_{forbidden}_{jobs}"
            output = self.create(name, command="run", workers=workers,
                                 forbidden_pairs=forbidden, chunk_size=size, jobs=jobs,
                                 write_profiles=profiles, merge_profiles=profiles)
            expected = self.root / ("direct_" + name)
            self.direct(expected, workers, forbidden, profiles=profiles)
            self.compare_outputs(expected, output / "merged", profiles)
            completion = json.loads((output / "merged/completion.json").read_text())
            self.assertTrue(completion["whole_domain_complete"])
            self.assertEqual(completion["elapsed_seconds_basis"], "sum_chunk_active_seconds")
            self.assertTrue(json.loads(self.command("status", "--output", output, "--json").stdout)["merged_complete"])
            before = {str(p): p.stat().st_mtime_ns for p in (output / "chunks").glob("*/completion.json")}
            extra = ["--merge-profiles"] if profiles else []
            self.command("run", "--output", output, "--jobs", 2, *extra)
            (output / "merged/summary.csv").unlink()
            (output / "merged/progress.json").unlink()
            self.command("merge", "--output", output, *extra)
            self.assertTrue((output / "merged/progress.json").exists())
            self.assertEqual(before, {str(p): p.stat().st_mtime_ns for p in (output / "chunks").glob("*/completion.json")})
            self.compare_outputs(expected, output / "merged", profiles)
            self.assertEqual(bool((output / "merged/profiles.csv").exists()), profiles)

    def test_bounded_run_skips_completed_chunks_then_resumes(self):
        output = self.create("bounded", command="run", chunk_size=100, jobs=2, stop_after_chunks=2)
        current = json.loads(self.command("status", "--output", output, "--json").stdout)
        self.assertEqual(current["completed_chunks"], 2)
        self.assertEqual(current["processed_representative_inputs"], 200)
        self.assertFalse((output / "merged").exists())
        completed = {str(p): p.stat().st_mtime_ns for p in (output / "chunks").glob("*/checkpoint.json")}
        self.command("run", "--output", output, "--jobs", 3)
        for name, modified in completed.items():
            self.assertEqual(Path(name).stat().st_mtime_ns, modified)
        self.assertTrue((output / "merged/completion.json").exists())

    def test_resume_partial_worker_truncates_uncommitted_profiles(self):
        output = self.create("partial_worker", start_id=400, count=180, chunk_size=60, write_profiles=True)
        plan = manager.Plan.load(output)
        first = next(plan.chunks())
        directory = plan.directory(first)
        self.direct(directory, start=400, count=60, profiles=True, stop=7)
        with (directory / "profiles.csv").open("a") as stream:
            stream.write("uncommitted data\n")
        self.command("run", "--output", output, "--jobs", 2, "--merge-profiles")
        self.assertIn('"--resume"', plan.log(first).read_text())
        expected = self.root / "partial_expected"
        self.direct(expected, start=400, count=180, profiles=True)
        self.compare_outputs(expected, output / "merged", True)
        self.assertFalse(json.loads((output / "merged/completion.json").read_text())["whole_domain_complete"])

    def test_merge_refuses_incomplete_corrupted_or_changed_ranges(self):
        output = self.create("incomplete", command="run", chunk_size=100, jobs=2, stop_after_chunks=1)
        self.command("merge", "--output", output, success=False)
        self.assertFalse((output / "merged").exists())
        output = self.create("completed", command="run", chunk_size=100, jobs=3, no_merge=True)
        damaged = self.root / "damaged"
        shutil.copytree(output, damaged)
        cp = damaged / "chunks/part_000000/checkpoint.json"
        data = json.loads(cp.read_text())
        data["payload"]["state"]["processed"] += 1
        cp.write_text(json.dumps(data))
        self.command("merge", "--output", damaged, success=False)
        self.assertFalse((damaged / "merged").exists())
        changed = self.root / "changed"
        shutil.copytree(output, changed)
        path = changed / "chunks.csv"
        path.write_text(path.read_text().replace("0,0,100,100,", "0,1,100,99,"))
        self.command("merge", "--output", changed, success=False)
        config_path = output / "chunks/part_000000/config.json"
        data = json.loads(config_path.read_text())
        data["capacities"] = [2, 1]
        config_path.write_text(json.dumps(data))
        self.command("merge", "--output", output, success=False)

    def test_profile_merge_rejects_reordered_rows_without_partial_output(self):
        output = self.create("bad_profiles", command="run", chunk_size=137, jobs=2,
                             write_profiles=True, no_merge=True)
        profile = output / "chunks/part_000000/profiles.csv"
        lines = profile.read_bytes().splitlines(keepends=True)
        lines[1], lines[2] = lines[2], lines[1]
        profile.write_bytes(b"".join(lines))
        self.command("merge", "--output", output, "--merge-profiles", success=False)
        self.assertFalse((output / "merged").exists())
        self.assertFalse(list(output.glob(".merge-tmp-*")))

    def test_cancelled_merge_keeps_completed_chunks_and_removes_temporary_output(self):
        output = self.create("cancel_merge", command="run", count=100, chunk_size=30, jobs=2,
                             write_profiles=True, no_merge=True)
        plan = manager.Plan.load(output)
        with self.assertRaises(manager.MergeInterrupted):
            manager.merge(plan, include_profiles=True,
                          cancelled=lambda: bool(list(output.glob(".merge-tmp-*"))))
        self.assertFalse((output / "merged").exists())
        self.assertFalse(list(output.glob(".merge-tmp-*")))
        self.assertEqual(len(list((output / "chunks").glob("*/completion.json"))), 4)
        self.command("merge", "--output", output, "--merge-profiles")
        self.assertTrue((output / "merged/completion.json").exists())

    def test_locks_and_initialization_recovery(self):
        output = self.create("locked", count=20, chunk_size=10)
        plan = manager.Plan.load(output)
        with (output / ".manager.lock").open("a") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.command("run", "--output", output, success=False)
            self.command("merge", "--output", output, success=False)
        chunk = next(plan.chunks())
        directory = plan.directory(chunk)
        directory.mkdir()
        (directory / "preserved.txt").write_text("keep this interrupted initialization")
        with (directory / ".run.lock").open("a") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            result = self.command("run", "--output", output, success=False)
            self.assertIn("already running outside", result.stderr)
        # Unowned directories are never silently removed.
        self.command("run", "--output", output, success=False)
        runner.atomic_json(plan.marker(chunk), dict(plan_sha256=plan.sha256))
        self.command("run", "--output", output, "--jobs", 2)
        backups = list((output / "initialization_backups").glob("*/preserved.txt"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(), "keep this interrupted initialization")

    def test_invalid_inputs_do_not_create_plans(self):
        for extra in (["--chunk-size", 0], ["--count", 0], ["--jobs", 0],
                      ["--status-every", "nan"], ["--merge-profiles"], ["--plan-limit", 0]):
            output = self.root / "invalid"
            self.command("run", "--binary", BINARY, "--firms", 2, "--workers", 2,
                         "--capacities", "1,2", "--forbidden-pairs", 1,
                         "--output", output, *extra, success=False)
            self.assertFalse(output.exists())
        output = self.create("immutable")
        self.command("run", "--output", output, "--chunk-size", 1, success=False)
        self.command("run", "--output", output, "--capacities", "2,1", success=False)
        self.command("plan", "--binary", BINARY, "--firms", 3, "--workers", 5,
                     "--capacities", "1,2,2", "--forbidden-pairs", 4,
                     "--output", self.root / "too_many", success=False)
        self.assertFalse((self.root / "too_many").exists())

    def test_worker_failure_is_reported_and_can_be_retried(self):
        build = self.root / "transient_engine"
        build.mkdir()
        binary = build / "run_rda_exhaustive"
        binary.write_text("test-only launcher placeholder")
        engine = build / "rda_stability_engine"
        real_engine = str(BINARY.with_name("rda_stability_engine"))
        engine.write_text(
            f"#!{sys.executable}\n"
            "import os,sys\nfrom pathlib import Path\n"
            "if not Path(__file__).with_suffix('.ready').exists():\n    sys.exit(7)\n"
            f"os.execv({real_engine!r}, [{real_engine!r}, *sys.argv[1:]])\n")
        engine.chmod(0o755)
        output = self.root / "retry"
        result = self.command(
            "run", "--binary", binary, "--firms", 2, "--workers", 2,
            "--capacities", "1,2", "--forbidden-pairs", 1,
            "--count", 100, "--chunk-size", 25, "--jobs", 2,
            "--output", output, success=False)
        self.assertIn("worker failed", result.stderr)
        self.assertFalse((output / "merged").exists())
        saved = json.loads((output / "manager_status.json").read_text())
        self.assertEqual(saved["status"], "failed")
        self.assertTrue(saved["failed_chunks"])
        engine.with_suffix(".ready").touch()
        self.command("run", "--output", output, "--jobs", 2)
        self.assertEqual(json.loads((output / "merged/completion.json").read_text())[
            "processed_representative_inputs"], 100)

    def test_sigterm_stops_workers_and_preserves_their_checkpoints(self):
        output = self.root / "signal"
        process = subprocess.Popen([
            sys.executable, str(MANAGER), "run", "--binary", str(BINARY),
            "--firms", "3", "--workers", "4", "--capacities", "1,1,2",
            "--forbidden-pairs", "2", "--count", "200000", "--chunk-size", "100000",
            "--progress-every", "1000", "--status-every", "0.05", "--jobs", "2",
            "--output", str(output)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        observed = []
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                path = output / "manager_status.json"
                if path.exists():
                    current = json.loads(path.read_text())
                    observed.append(len(current["running_chunks"]))
                    if current["processed_representative_inputs"] >= 1000:
                        break
                time.sleep(0.01)
            else:
                self.fail("manager did not publish worker progress")
            process.send_signal(signal.SIGTERM)
            stdout, stderr = process.communicate(timeout=30)
            self.assertEqual(process.returncode, 128 + signal.SIGTERM, stdout + stderr)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.communicate(timeout=30)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate()
        self.assertTrue(observed)
        self.assertLessEqual(max(observed), 2)
        current = json.loads(self.command("status", "--output", output, "--json").stdout)
        self.assertEqual(current["running_chunks"], [])
        self.assertEqual(current["invalid_chunks"], [])
        self.assertGreaterEqual(current["processed_representative_inputs"], 1000)
        self.assertFalse((output / "merged").exists())
        self.command("run", "--output", output, "--jobs", 2)
        self.assertIn('"--resume"', (output / "logs/part_000000.log").read_text())
        completion = json.loads((output / "merged/completion.json").read_text())
        self.assertEqual(completion["processed_representative_inputs"], 200000)
        self.assertFalse(completion["whole_domain_complete"])


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
