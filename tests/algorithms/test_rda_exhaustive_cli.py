"""End-to-end range, output, and merge checks; pass the compiled runner as argv[1]."""

import csv
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


EXPERIMENT = Path(__file__).resolve().parents[2] / "experiments" / "universal_replacement"
SPEC = importlib.util.spec_from_file_location("merge_rda_exhaustive", EXPERIMENT / "merge_rda_exhaustive.py")
MERGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MERGER)
BINARY = None


class RDAExhaustiveCLI(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rda-exhaustive-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def command(self, *args, success=True):
        result = subprocess.run([str(BINARY), *map(str, args)], capture_output=True, text=True, timeout=60)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def run_range(self, name, workers=3, start=0, count=None, profiles=False):
        output = self.root / name
        arguments = ["--workers", workers, "--start-id", start, "--output", output, "--progress-every", 200000]
        if count is not None:
            arguments += ["--count", count]
        if profiles:
            arguments += ["--write-profiles"]
        self.command(*arguments)
        return output

    def test_counts_and_invalid_ranges_do_not_create_output(self):
        for workers, expected in [(3, 995328), (4, 1194393600)]:
            result = self.command("--workers", workers, "--dry-run")
            self.assertIn(f"total_inputs={expected}", result.stdout)
        for arguments in [
            ["--workers", 2], ["--workers", 3, "--count", 0],
            ["--workers", 3, "--start-id", 995328],
            ["--workers", 3, "--start-id", 995327, "--count", 2],
            ["--workers", 4, "--count", "-1"],
            ["--workers", 4, "--count", "18446744073709551616"],
            ["--workers", 3, "--progress-every", 0],
        ]:
            output = self.root / "invalid"
            self.command(*arguments, "--output", output, success=False)
            self.assertFalse(output.exists())
        self.command("--workers", 3, "--count", 1, "--output", self.root, success=False)
        self.assertFalse((self.root / "config.json").exists())

    def test_complete_domain_equals_merged_disjoint_ranges_and_can_be_remerged(self):
        whole = self.run_range("whole")
        left = self.run_range("left", count=400000)
        right = self.run_range("right", start=400000)
        merged = self.root / "merged"
        MERGER.merge([right, left], merged, require_full=True)
        for name in ["summary.csv", "examples.jsonl"]:
            if name.endswith("csv"):
                with (whole / name).open(newline="") as stream:
                    expected = list(csv.DictReader(stream))
                with (merged / name).open(newline="") as stream:
                    actual = list(csv.DictReader(stream))
                self.assertEqual(actual, expected)
            else:
                load = lambda directory: sorted(
                    [json.loads(line) for line in (directory / name).read_text().splitlines()],
                    key=lambda row: row["instance_id"],
                )
                self.assertEqual(load(whole), load(merged))
        completed = MERGER.read_json(whole / "completion.json")
        self.assertTrue(completed["whole_domain_complete"])
        self.assertEqual(completed["processed_inputs"], 995328)
        self.assertFalse((whole / "profiles.csv").exists())
        MERGER.merge([merged], self.root / "remerged", require_full=True)
        self.assertEqual((merged / "summary.csv").read_text(), (self.root / "remerged" / "summary.csv").read_text())

    def test_replay_profiles_witnesses_partial_ranges_and_merge_rejections(self):
        # A nontrivial mixed result: three leaves, two stable, and two deletions.
        replay = self.run_range("replay", start=398019, count=1, profiles=True)
        with (replay / "profiles.csv").open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["category"], "mixed")
        self.assertEqual(int(rows[0]["leaves"]), 3)
        self.assertEqual(int(rows[0]["stable_leaves"]), 2)
        example = json.loads((replay / "examples.jsonl").read_text())
        self.assertTrue(example["stable_witness"]["original_ur_stable"])
        self.assertFalse(example["unstable_witness"]["original_ur_stable"])
        self.assertTrue(example["unstable_witness"]["blocking_pairs"])
        tail = self.run_range("tail", workers=4, start=1194393599, count=1, profiles=True)
        self.assertFalse(MERGER.read_json(tail / "completion.json")["whole_domain_complete"])
        with self.assertRaisesRegex(ValueError, "overlap"):
            MERGER.merge([replay, replay], self.root / "overlap")
        with self.assertRaisesRegex(ValueError, "entire domain"):
            MERGER.merge([replay], self.root / "gap", require_full=True)
        with self.assertRaisesRegex(ValueError, "different experiment"):
            MERGER.merge([replay, tail], self.root / "different")
        (tail / "completion.json").unlink()
        with self.assertRaises(FileNotFoundError):
            MERGER.merge([tail], self.root / "incomplete")
        self.assertFalse((self.root / "overlap").exists())
        self.assertFalse((self.root / "gap").exists())
        self.assertFalse((self.root / "different").exists())
        self.assertFalse((self.root / "incomplete").exists())


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("Usage: python3 test_rda_exhaustive_cli.py /absolute/path/to/run_rda_exhaustive")
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
