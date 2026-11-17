#!/usr/bin/env python3
"""Plan, run, resume and merge disjoint three-stability RDA input ranges."""

import argparse
import csv
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

import run_rda_stability_exhaustive as runner


SCHEMA = "rda_chunk_manager_v1"
HERE = Path(__file__).resolve().parent
DEFAULT_BINARY = HERE.parents[1] / "build-release/experiments/universal_replacement/run_rda_exhaustive"
CHUNK_HEADER = ["chunk_id", "start_id", "end_id_exclusive", "count", "output_directory"]


class MergeInterrupted(RuntimeError):
    pass


def check_cancelled(cancelled):
    if cancelled is not None and cancelled():
        raise MergeInterrupted("merge stopped; source chunks are retained")


def now():
    return datetime.now(timezone.utc).isoformat()


def checksum(value):
    return hashlib.sha256(runner.encode_json(value).encode()).hexdigest()


def file_checksum(path, cancelled=None):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            check_cancelled(cancelled)
            result.update(block)
    return result.hexdigest()


def is_locked(path):
    try:
        with path.open("r") as stream:
            try:
                fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return True
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
    except FileNotFoundError:
        pass
    return False


class Plan:
    def __init__(self, root, data):
        self.root, self.data = Path(root).resolve(), data
        self.config = data["config"]
        self.sha256 = checksum(data)

    def chunks(self):
        for i in range(self.data["chunk_count"]):
            start = self.config["start_id"] + i * self.data["chunk_size"]
            end = min(start + self.data["chunk_size"], self.config["end_id_exclusive"])
            yield dict(chunk_id=i, start_id=start, end_id_exclusive=end,
                       count=end - start, output_directory=f"chunks/part_{i:06d}")

    def chunk_config(self, chunk):
        return dict(self.config, start_id=chunk["start_id"], end_id_exclusive=chunk["end_id_exclusive"])

    def directory(self, chunk):
        return self.root / chunk["output_directory"]

    def log(self, chunk):
        return self.root / "logs" / f'part_{chunk["chunk_id"]:06d}.log'

    def marker(self, chunk):
        return self.root / "logs" / f'part_{chunk["chunk_id"]:06d}.attempt.json'

    @classmethod
    def load(cls, root):
        root = Path(root).resolve()
        envelope = json.loads((root / "plan.json").read_text())
        if checksum(envelope["plan"]) != envelope["sha256"]:
            raise ValueError("plan.json checksum mismatch")
        plan = cls(root, envelope["plan"])
        if plan.data["schema_version"] != SCHEMA or plan.config["schema_version"] != runner.SCHEMA:
            raise ValueError("unsupported plan or runner schema")
        domain = runner.Domain(plan.config["n_firms"], plan.config["n_workers"],
                               plan.config["capacities"], plan.config["forbidden_pairs"])
        for key, value in domain.metadata().items():
            if plan.config[key] != value:
                raise ValueError(f"plan domain metadata mismatch: {key}")
        start, end = plan.config["start_id"], plan.config["end_id_exclusive"]
        size = plan.data["chunk_size"]
        if not 0 <= start < end <= domain.total or size < 1:
            raise ValueError("invalid planned range or chunk size")
        if plan.data["chunk_count"] != (end - start + size - 1) // size:
            raise ValueError("chunk count does not cover the planned range")
        manifest = root / "chunks.csv"
        if runner.digest(manifest) != plan.data["chunks_sha256"]:
            raise ValueError("chunks.csv checksum mismatch")
        with manifest.open(newline="") as stream:
            reader = csv.DictReader(stream)
            if reader.fieldnames != CHUNK_HEADER:
                raise ValueError("unexpected chunks.csv header")
            for expected in plan.chunks():
                actual = next(reader, None)
                if actual != {key: str(expected[key]) for key in CHUNK_HEADER}:
                    raise ValueError("chunks.csv has a missing, overlapping or altered range")
            if next(reader, None) is not None:
                raise ValueError("chunks.csv has unexpected extra ranges")
        return plan


def build_plan(args):
    if None in (args.firms, args.workers, args.capacities, args.forbidden_pairs):
        raise ValueError("new plans require --firms, --workers, --capacities and --forbidden-pairs")
    binary = (args.binary or DEFAULT_BINARY).resolve()
    engine = binary.with_name("rda_stability_engine")
    if not binary.is_file() or not engine.is_file():
        raise ValueError("runner/engine not found; build run_rda_exhaustive or pass --binary")
    if not os.access(engine, os.X_OK):
        raise ValueError("rda_stability_engine is not executable")
    size = args.chunk_size if args.chunk_size is not None else 100000000
    interval = args.progress_every if args.progress_every is not None else 100000
    if size < 1 or interval < 1 or args.plan_limit < 1:
        raise ValueError("chunk-size, progress-every and plan-limit must be positive")
    domain, config = runner.new_config(args, runner.digest(engine))
    chunk_count = (config["end_id_exclusive"] - config["start_id"] + size - 1) // size
    if chunk_count > args.plan_limit:
        raise ValueError(f"plan would contain {chunk_count} chunks (limit {args.plan_limit}); "
                         "increase --chunk-size, select --count, or explicitly raise --plan-limit")
    data = dict(schema_version=SCHEMA, config=config, chunk_size=size, chunk_count=chunk_count,
                progress_every=interval, binary=str(binary), engine=str(engine),
                manager_sha256=runner.digest(__file__), created_at=now())
    plan = Plan(args.output, data)
    if getattr(args, "dry_run", False):
        print(runner.encode_json(dict(**domain.metadata(), chunk_size=size, chunk_count=chunk_count,
                                      start_id=config["start_id"], end_id_exclusive=config["end_id_exclusive"])))
        return plan
    plan.root.mkdir(parents=True, exist_ok=False)
    (plan.root / "chunks").mkdir()
    (plan.root / "logs").mkdir()
    temporary = plan.root / "chunks.csv.tmp"
    with temporary.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, CHUNK_HEADER, lineterminator="\n")
        writer.writeheader()
        writer.writerows(plan.chunks())
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(plan.root / "chunks.csv")
    data["chunks_sha256"] = runner.digest(plan.root / "chunks.csv")
    plan = Plan(plan.root, data)
    runner.atomic_json(plan.root / "plan.json", dict(sha256=plan.sha256, plan=data))
    print(f"Plan: {plan.root}\nChunks: {chunk_count}\n"
          f'Representatives: {config["end_id_exclusive"] - config["start_id"]}', flush=True)
    return plan


def assert_existing_options(args, plan):
    for key, value in (("n_firms", args.firms), ("n_workers", args.workers),
                       ("forbidden_pairs", args.forbidden_pairs), ("start_id", args.start_id),
                       ("write_profiles", args.write_profiles)):
        if value is not None and plan.config[key] != value:
            raise ValueError(f"option conflicts with saved plan: {key}")
    if args.capacities is not None and list(map(int, args.capacities.split(","))) != plan.config["capacities"]:
        raise ValueError("capacities conflict with saved plan")
    if args.count is not None and args.count != plan.config["end_id_exclusive"] - plan.config["start_id"]:
        raise ValueError("count conflicts with saved plan")
    if args.chunk_size is not None and args.chunk_size != plan.data["chunk_size"]:
        raise ValueError("chunk-size conflicts with saved plan")


def nonnegative_integer(value):
    return type(value) is int and value >= 0


def validate_state(config, state):
    """Check independent forest/class/pattern accounting before any aggregation."""
    processed = state["processed"]
    if not nonnegative_integer(processed):
        raise ValueError("invalid processed count")
    if len(state["summaries"]) != 3:
        raise ValueError("expected three stability summaries")
    for key in ("max_nodes", "max_deletions", "profiles_offset"):
        if not nonnegative_integer(state[key]):
            raise ValueError("invalid maximum/offset counter")
    for row in state["summaries"]:
        if any(not nonnegative_integer(row[key]) for key in runner.SUMMARY_COUNTS):
            raise ValueError("invalid summary counter")
        if row["inputs"] != processed or row["stable_exists"] + row["stable_not_exists"] != processed:
            raise ValueError("summary input/existence accounting mismatch")
        if sum(row["rda_" + cat] for cat in runner.CATEGORIES) != processed:
            raise ValueError("summary RDA category accounting mismatch")
    expected = [dict.fromkeys(runner.SUMMARY_COUNTS, 0) for _ in range(3)]
    by_conflicts = {}
    for key, count in state["classes"].items():
        parts = list(map(int, key.split(":")))
        if len(parts) != 7 or parts[0] < 0 or not all(x in (0, 1) for x in parts[1:4]) \
                or not all(x in (0, 1, 2) for x in parts[4:]) or type(count) is not int or count < 1:
            raise ValueError("invalid joint class")
        by_conflicts[parts[0]] = by_conflicts.get(parts[0], 0) + count
        for d in range(3):
            row = expected[d]
            exists, category = parts[1 + d], parts[4 + d]
            if not exists and category != 0:
                raise ValueError("stable RDA leaf without an existing stable matching")
            row["inputs"] += count
            row["stable_exists"] += count * exists
            row["stable_not_exists"] += count * (1 - exists)
            row["rda_" + runner.CATEGORIES[category]] += count
            row["exists_but_unreached"] += count * int(exists and category == 0)
    for d in range(3):
        for key in runner.SUMMARY_COUNTS[:7]:
            if expected[d][key] != state["summaries"][d][key]:
                raise ValueError("joint classes disagree with summary")
    paths = distinct = 0
    stable_paths, stable_distinct = [0] * 3, [0] * 3
    for key, values in state["patterns"].items():
        conflicts, pattern = map(int, key.split(":"))
        if not 0 <= pattern < 8 or len(values) != 3 or any(not nonnegative_integer(x) for x in values):
            raise ValueError("invalid leaf pattern")
        inputs, leaves, matchings = values
        if not 0 < inputs <= by_conflicts.get(conflicts, 0) or not inputs <= matchings <= leaves:
            raise ValueError("invalid leaf pattern counts")
        paths += leaves
        distinct += matchings
        for d in range(3):
            if pattern & (1 << d):
                stable_paths[d] += leaves
                stable_distinct[d] += matchings
    if any(not nonnegative_integer(state["stats"][key]) for key in runner.STATS):
        raise ValueError("invalid search counters")
    if paths != state["stats"]["leaves"] or distinct != state["stats"]["distinct_matchings"]:
        raise ValueError("leaf patterns disagree with search counters")
    if state["stats"]["nodes"] != state["stats"]["branches"] + processed:
        raise ValueError("RDA forest accounting mismatch")
    if state["stats"]["leaves"] < processed or state["max_nodes"] > state["stats"]["nodes"] \
            or state["max_deletions"] > config["n_firms"] * config["n_workers"]:
        raise ValueError("invalid RDA search bounds")
    for d, row in enumerate(state["summaries"]):
        if row["stable_leaves"] != stable_paths[d] or row["distinct_stable_matchings"] != stable_distinct[d]:
            raise ValueError("leaf patterns disagree with stability summary")
        if row["stable_solution_counts"] < max(row["stable_exists"], stable_distinct[d]):
            raise ValueError("stable solution count below stable witness count")
    if set(state["examples"]) != set(state["classes"]):
        raise ValueError("missing or unexpected representative example")
    for key, example in state["examples"].items():
        problem, result = example["input"], example["result"]
        if not config["start_id"] <= problem["instance_id"] < state["next_id"] \
                or problem["symmetry_weight"] != config["symmetry_weight"] \
                or result["instance_id"] != problem["instance_id"] \
                or example["existence_search_complete"] is not True:
            raise ValueError("invalid example identity")
        actual_key = ":".join(map(str, [
            result["initial_conflicts"], *[int(c > 0) for c in result["stable_solution_counts"]],
            *runner.classify(result)]))
        if key != actual_key:
            raise ValueError("example does not belong to its class")
    for key in ("elapsed_seconds", "rda_seconds", "existence_seconds"):
        if not isinstance(state[key], (int, float)) or not math.isfinite(state[key]) or state[key] < 0:
            raise ValueError("invalid elapsed time")


def inspect_chunk(plan, chunk, require_complete=False):
    directory = plan.directory(chunk)
    if not directory.exists():
        if require_complete:
            raise ValueError(f'missing chunk {chunk["chunk_id"]}')
        return None
    if not (directory / "checkpoint.json").exists():
        if require_complete:
            raise ValueError(f'chunk {chunk["chunk_id"]} has no checkpoint')
        return None
    config, state = runner.load_checkpoint(directory)
    if config != plan.chunk_config(chunk):
        raise ValueError(f'chunk {chunk["chunk_id"]} configuration differs from plan')
    validate_state(config, state)
    complete = (directory / "completion.json").exists()
    if require_complete and not complete:
        raise ValueError(f'chunk {chunk["chunk_id"]} is incomplete')
    if complete:
        completion = json.loads((directory / "completion.json").read_text())
        checkpoint = json.loads((directory / "checkpoint.json").read_text())
        progress = json.loads((directory / "progress.json").read_text())
        if completion.get("schema_version") != runner.SCHEMA or completion.get("complete") is not True \
                or state["processed"] != chunk["count"] or state["next_id"] != chunk["end_id_exclusive"] \
                or completion.get("processed_representative_inputs") != state["processed"] \
                or completion.get("processed_labeled_inputs") != state["processed"] * config["symmetry_weight"] \
                or completion.get("next_id") != state["next_id"] \
                or completion.get("checkpoint_sha256") != checkpoint["sha256"] \
                or progress.get("checkpoint_sha256") != checkpoint["sha256"] \
                or progress.get("processed_representative_inputs") != state["processed"] \
                or progress.get("next_id") != state["next_id"]:
            raise ValueError(f'chunk {chunk["chunk_id"]} completion/checkpoint mismatch')
        if config["write_profiles"] and (directory / "profiles.csv").stat().st_size != state["profiles_offset"]:
            raise ValueError(f'chunk {chunk["chunk_id"]} profiles.csv size mismatch')
    return dict(state=state, complete=complete,
                checkpoint_sha256=json.loads((directory / "checkpoint.json").read_text())["sha256"])


def combine_state(aggregate, state):
    aggregate["processed"] += state["processed"]
    aggregate["next_id"] = state["next_id"]
    for d, row in enumerate(state["summaries"]):
        for key in runner.SUMMARY_COUNTS:
            aggregate["summaries"][d][key] += row[key]
    for key, count in state["classes"].items():
        aggregate["classes"][key] = aggregate["classes"].get(key, 0) + count
    for key, values in state["patterns"].items():
        old = aggregate["patterns"].setdefault(key, [0, 0, 0])
        for i in range(3):
            old[i] += values[i]
    for key, example in state["examples"].items():
        old = aggregate["examples"].get(key)
        if old is None or example["input"]["instance_id"] < old["input"]["instance_id"]:
            aggregate["examples"][key] = example
    for key in runner.STATS:
        aggregate["stats"][key] += state["stats"][key]
    for key in ("max_nodes", "max_deletions"):
        aggregate[key] = max(aggregate[key], state[key])
    for key in ("elapsed_seconds", "rda_seconds", "existence_seconds"):
        aggregate[key] += state[key]


def merge_profiles(plan, temporary, cancelled=None):
    offset = 0
    with (temporary / "profiles.csv").open("w", newline="") as target:
        writer = csv.writer(target, lineterminator="\n")
        writer.writerow(runner.PROFILE_HEADER)
        for chunk in plan.chunks():
            check_cancelled(cancelled)
            with (plan.directory(chunk) / "profiles.csv").open(newline="") as source:
                reader = csv.reader(source)
                if next(reader, None) != runner.PROFILE_HEADER:
                    raise ValueError(f'chunk {chunk["chunk_id"]} has an unexpected profiles header')
                expected = chunk["start_id"]
                for row in reader:
                    if (expected - chunk["start_id"]) % 10000 == 0:
                        check_cancelled(cancelled)
                    if len(row) != len(runner.PROFILE_HEADER) or int(row[0]) != expected \
                            or int(row[1]) != plan.config["symmetry_weight"]:
                        raise ValueError(f'chunk {chunk["chunk_id"]} has noncontiguous profile rows')
                    if expected >= chunk["end_id_exclusive"]:
                        raise ValueError("extra profile rows")
                    writer.writerow(row)
                    expected += 1
                if expected != chunk["end_id_exclusive"]:
                    raise ValueError("missing profile rows")
        target.flush()
        os.fsync(target.fileno())
        offset = target.tell()
    return offset


def merge(plan, include_profiles=False, cancelled=None):
    if include_profiles and not plan.config["write_profiles"]:
        raise ValueError("--merge-profiles requires a plan created with --write-profiles")
    aggregate = runner.initial_state(plan.config["start_id"])
    sources = []
    for chunk in plan.chunks():
        check_cancelled(cancelled)
        if is_locked(plan.directory(chunk) / ".run.lock"):
            raise ValueError(f'chunk {chunk["chunk_id"]} is still running; merge after it exits')
        inspected = inspect_chunk(plan, chunk, require_complete=True)
        combine_state(aggregate, inspected["state"])
        source = dict(chunk_id=chunk["chunk_id"], start_id=chunk["start_id"],
                      end_id_exclusive=chunk["end_id_exclusive"],
                      checkpoint_sha256=inspected["checkpoint_sha256"])
        if include_profiles:
            source["profiles_sha256"] = file_checksum(plan.directory(chunk) / "profiles.csv", cancelled)
        sources.append(source)
    if aggregate["processed"] != plan.config["end_id_exclusive"] - plan.config["start_id"]:
        raise ValueError("merged inputs do not cover the planned range")
    config = dict(plan.config, write_profiles=include_profiles, source_write_profiles=plan.config["write_profiles"],
                  elapsed_seconds_basis="sum_chunk_active_seconds", merged=True, plan_sha256=plan.sha256)
    validate_state(config, aggregate)
    provenance = dict(schema_version=SCHEMA, plan_sha256=plan.sha256,
                      merge_profiles=include_profiles, chunks=sources)
    target = plan.root / "merged"
    if target.exists():
        saved = json.loads((target / "merge_manifest.json").read_text())
        if include_profiles:
            if file_checksum(target / "profiles.csv", cancelled) != saved.get("profiles_sha256"):
                raise ValueError("existing merged profiles.csv checksum mismatch")
            provenance["profiles_sha256"] = saved["profiles_sha256"]
        if saved != provenance:
            raise ValueError("existing merged results have different sources/options; refusing overwrite")
        saved_config, saved_state = runner.load_checkpoint(target)
        validate_state(saved_config, saved_state)
        if include_profiles and (target / "profiles.csv").stat().st_size != saved_state["profiles_offset"]:
            raise ValueError("existing merged profiles.csv size mismatch")
        expected_state = dict(aggregate, profiles_offset=saved_state["profiles_offset"])
        if saved_config != config or saved_state != expected_state:
            raise ValueError("existing merged checkpoint differs from source chunks")
        completion = json.loads((target / "completion.json").read_text())
        cp = json.loads((target / "checkpoint.json").read_text())
        if completion.get("complete") is not True or completion.get("checkpoint_sha256") != cp["sha256"] \
                or completion.get("processed_representative_inputs") != aggregate["processed"] \
                or completion.get("plan_sha256") != plan.sha256:
            raise ValueError("existing merged completion is invalid")
        # Repair published files from the validated authoritative state. The
        # deterministic checkpoint checksum stays unchanged.
        runner.save_checkpoint(target, saved_config, saved_state, None)
        return target
    temporary = Path(tempfile.mkdtemp(prefix=".merge-tmp-", dir=plan.root))
    try:
        if include_profiles:
            aggregate["profiles_offset"] = merge_profiles(plan, temporary, cancelled)
            provenance["profiles_sha256"] = file_checksum(temporary / "profiles.csv", cancelled)
        check_cancelled(cancelled)
        runner.atomic_json(temporary / "config.json", config)
        runner.save_checkpoint(temporary, config, aggregate, None)
        runner.atomic_json(temporary / "merge_manifest.json", provenance)
        cp = json.loads((temporary / "checkpoint.json").read_text())
        runner.atomic_json(temporary / "completion.json", dict(
            schema_version=runner.SCHEMA, complete=True,
            whole_domain_complete=config["start_id"] == 0
                and config["end_id_exclusive"] == config["total_representative_inputs"],
            processed_representative_inputs=aggregate["processed"],
            processed_labeled_inputs=aggregate["processed"] * config["symmetry_weight"],
            next_id=aggregate["next_id"], elapsed_seconds=aggregate["elapsed_seconds"],
            elapsed_seconds_basis="sum_chunk_active_seconds", source_chunks=len(sources),
            plan_sha256=plan.sha256, checkpoint_sha256=cp["sha256"]))
        temporary.rename(target)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
    return target


@contextmanager
def manager_lock(plan):
    with (plan.root / ".manager.lock").open("a") as stream:
        try:
            fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError("another manager/merger is already using this output directory")
        yield


def read_progress(plan, chunk):
    path = plan.directory(chunk) / "progress.json"
    if not path.exists():
        return 0
    progress = json.loads(path.read_text())
    count = progress["processed_representative_inputs"]
    if not nonnegative_integer(count) or count > chunk["count"] \
            or progress["next_id"] != chunk["start_id"] + count \
            or progress["processed_labeled_inputs"] != count * plan.config["symmetry_weight"]:
        raise ValueError(f'chunk {chunk["chunk_id"]} progress is outside its range')
    return count


def inventory(plan):
    completed, running, invalid, processed = set(), [], [], {}
    for chunk in plan.chunks():
        i = chunk["chunk_id"]
        try:
            if is_locked(plan.directory(chunk) / ".run.lock"):
                running.append(i)
                processed[i] = read_progress(plan, chunk)
            else:
                inspected = inspect_chunk(plan, chunk)
                processed[i] = inspected["state"]["processed"] if inspected else 0
                if inspected and inspected["complete"]:
                    completed.add(i)
        except (ValueError, OSError, KeyError, TypeError) as error:
            invalid.append(dict(chunk_id=i, error=str(error)))
            processed[i] = 0
    return completed, running, invalid, processed


def status(plan):
    completed, running, invalid, processed = inventory(plan)
    selected = plan.config["end_id_exclusive"] - plan.config["start_id"]
    active_manager = is_locked(plan.root / ".manager.lock")
    merged_complete = False
    completion_file = plan.root / "merged/completion.json"
    if completion_file.exists():
        try:
            completion = json.loads(completion_file.read_text())
            config, state = runner.load_checkpoint(plan.root / "merged")
            validate_state(config, state)
            cp = json.loads((plan.root / "merged/checkpoint.json").read_text())
            merged_complete = completion.get("complete") is True \
                and completion.get("plan_sha256") == plan.sha256 \
                and config.get("plan_sha256") == plan.sha256 \
                and completion.get("checkpoint_sha256") == cp["sha256"] \
                and completion.get("processed_representative_inputs") == selected \
                and state["processed"] == selected
            if not merged_complete:
                raise ValueError("merged completion/checkpoint mismatch")
        except (ValueError, OSError, KeyError, TypeError) as error:
            invalid.append(dict(merged=True, error=str(error)))
    name = "running" if active_manager or running else (
        "invalid" if invalid else "complete" if merged_complete and len(completed) == plan.data["chunk_count"]
        else "chunks_complete" if len(completed) == plan.data["chunk_count"] else "stopped")
    result = dict(schema_version=SCHEMA, plan_sha256=plan.sha256, status=name,
                  total_chunks=plan.data["chunk_count"], completed_chunks=len(completed),
                  running_chunks=running, pending_chunks=plan.data["chunk_count"] - len(completed) - len(running),
                  invalid_chunks=invalid, processed_representative_inputs=sum(processed.values()),
                  processed_labeled_inputs=sum(processed.values()) * plan.config["symmetry_weight"],
                  selected_representative_inputs=selected, merged_complete=merged_complete)
    heartbeat = plan.root / "manager_status.json"
    if heartbeat.exists():
        saved = json.loads(heartbeat.read_text())
        if saved.get("plan_sha256") == plan.sha256:
            result["last_manager_status"] = saved
            if name == "stopped" and saved.get("status") == "failed":
                result["status"] = "failed"
    return result


def implementation(plan, binary_override=None):
    engine = ((binary_override.resolve().with_name("rda_stability_engine"))
              if binary_override else Path(plan.data["engine"]))
    if not engine.is_file() or not os.access(engine, os.X_OK):
        raise ValueError("planned C++ engine is unavailable; pass --binary from the original matching build")
    if runner.digest(engine) != plan.config["engine_sha256"] \
            or runner.digest(runner.__file__) != plan.config["driver_sha256"]:
        raise ValueError("implementation differs from the plan; use the original engine and Python driver")
    return engine


def prepare_initialization(plan, chunk):
    """Preserve a manager-owned directory left before the very first checkpoint."""
    directory = plan.directory(chunk)
    if not directory.exists() or (directory / "checkpoint.json").exists():
        return
    marker = plan.marker(chunk)
    if not marker.exists() or json.loads(marker.read_text()).get("plan_sha256") != plan.sha256:
        raise ValueError(f"existing directory without a checkpoint is not manager-owned: {directory}")
    if is_locked(directory / ".run.lock"):
        raise ValueError(f"chunk is already running: {directory}")
    backup = plan.root / "initialization_backups"
    backup.mkdir(exist_ok=True)
    destination = backup / f'part_{chunk["chunk_id"]:06d}_{time.time_ns()}'
    directory.rename(destination)
    print(f"Preserved interrupted initialization: {destination}", file=sys.stderr, flush=True)


def start_chunk(plan, chunk, engine, progress_every):
    implementation(plan, engine.with_name("run_rda_exhaustive"))
    directory = plan.directory(chunk)
    if is_locked(directory / ".run.lock"):
        raise ValueError(f"chunk is already running: {directory}")
    prepare_initialization(plan, chunk)
    command = [sys.executable, str(HERE / "run_rda_stability_exhaustive.py"),
               "--engine", str(engine), "--progress-every", str(progress_every)]
    if (directory / "checkpoint.json").exists():
        inspect_chunk(plan, chunk)
        command += ["--resume", str(directory)]
    else:
        command += [
            "--firms", str(plan.config["n_firms"]), "--workers", str(plan.config["n_workers"]),
            "--capacities", ",".join(map(str, plan.config["capacities"])),
            "--forbidden-pairs", str(plan.config["forbidden_pairs"]),
            "--start-id", str(chunk["start_id"]), "--count", str(chunk["count"]),
            "--output", str(directory)]
        if plan.config["write_profiles"]:
            command.append("--write-profiles")
    marker = dict(plan_sha256=plan.sha256, chunk_id=chunk["chunk_id"], started_at=now(), command=command)
    runner.atomic_json(plan.marker(chunk), marker)
    log = plan.log(chunk).open("a")
    try:
        log.write(f"\n# {now()} {runner.encode_json(command)}\n")
        log.flush()
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    except BaseException:
        log.close()
        raise
    marker["pid"] = process.pid
    try:
        runner.atomic_json(plan.marker(chunk), marker)
    except BaseException:
        process.terminate()
        process.wait(timeout=10)
        log.close()
        raise
    return dict(process=process, log=log, chunk=chunk)


def stop_children(active, timeout):
    """Ask each known Python worker to checkpoint; only kill it after timeout."""
    for item in active.values():
        if item["process"].poll() is None:
            item["process"].terminate()
    deadline = time.monotonic() + timeout
    while any(item["process"].poll() is None for item in active.values()) and time.monotonic() < deadline:
        time.sleep(0.1)
    for item in active.values():
        if item["process"].poll() is None:
            item["process"].kill()
        item["process"].wait(timeout=10)
        item["log"].close()


def supervise(plan, args):
    engine = implementation(plan, args.binary)
    completed, foreign_running, invalid, processed = inventory(plan)
    if foreign_running:
        raise ValueError(f"chunks are already running outside this manager: {foreign_running}; "
                         "wait for them or stop those workers before restarting the manager")
    if invalid:
        raise ValueError(f"invalid chunk data: {invalid}")
    old_status = plan.root / "manager_status.json"
    previous_seconds = 0.0
    if old_status.exists():
        previous = json.loads(old_status.read_text())
        if previous.get("plan_sha256") == plan.sha256:
            previous_seconds = previous.get("manager_active_seconds", 0.0)
    started_at = time.monotonic()
    active, failures, received_signals = {}, [], []
    old_handlers = {}
    progress_every = args.progress_every or plan.data["progress_every"]
    pending = iter(chunk for chunk in plan.chunks() if chunk["chunk_id"] not in completed)
    exhausted, launches = False, 0
    last_saved = 0.0
    last_rate_at, last_rate_count = started_at, sum(processed.values())
    rate = None

    def heartbeat(name, force=False):
        nonlocal last_saved, last_rate_at, last_rate_count, rate
        current = time.monotonic()
        if not force and current - last_saved < args.status_every:
            return
        for i, item in active.items():
            processed[i] = read_progress(plan, item["chunk"])
        total = sum(processed.values())
        if current - last_rate_at >= 1:
            rate = max(0, total - last_rate_count) / (current - last_rate_at)
            last_rate_at, last_rate_count = current, total
        selected = plan.config["end_id_exclusive"] - plan.config["start_id"]
        record = dict(
            schema_version=SCHEMA, plan_sha256=plan.sha256, status=name, updated_at=now(),
            manager_pid=os.getpid(), jobs=args.jobs, total_chunks=plan.data["chunk_count"],
            completed_chunks=len(completed), running_chunks=[
                dict(chunk_id=i, pid=item["process"].pid, committed_inputs=processed.get(i, 0))
                for i, item in sorted(active.items())],
            pending_chunks=plan.data["chunk_count"] - len(completed) - len(active),
            failed_chunks=failures, processed_representative_inputs=total,
            processed_labeled_inputs=total * plan.config["symmetry_weight"],
            selected_representative_inputs=selected,
            manager_active_seconds=previous_seconds + current - started_at,
            recent_representatives_per_second=rate,
            estimated_remaining_seconds=(selected - total) / rate if rate else None)
        runner.atomic_json(plan.root / "manager_status.json", record)
        print(f'{name}: chunks={len(completed)}/{plan.data["chunk_count"]} '
              f'running={len(active)} representatives={total}/{selected}', file=sys.stderr, flush=True)
        last_saved = current

    def request_stop(signum, frame):
        received_signals.append(signum)

    try:
        for signum in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            old_handlers[signum] = signal.signal(signum, request_stop)
        heartbeat("running", True)
        while active or not exhausted:
            if received_signals or failures:
                break
            changed = False
            while len(active) < args.jobs and not exhausted:
                if received_signals:
                    break
                if args.stop_after_chunks is not None and launches >= args.stop_after_chunks:
                    exhausted = True
                    break
                chunk = next(pending, None)
                if chunk is None:
                    exhausted = True
                    break
                active[chunk["chunk_id"]] = start_chunk(plan, chunk, engine, progress_every)
                launches += 1
                changed = True
            for i, item in list(active.items()):
                code = item["process"].poll()
                if code is None:
                    continue
                item["log"].close()
                del active[i]
                changed = True
                if code == 0:
                    try:
                        inspected = inspect_chunk(plan, item["chunk"], require_complete=True)
                        completed.add(i)
                        processed[i] = inspected["state"]["processed"]
                    except (ValueError, OSError, KeyError, TypeError) as error:
                        failures.append(dict(chunk_id=i, returncode=code, error=str(error), log=str(plan.log(item["chunk"]))))
                else:
                    failures.append(dict(chunk_id=i, returncode=code, log=str(plan.log(item["chunk"]))))
            heartbeat("running", changed)
            if active:
                time.sleep(0.1)
        if active:
            heartbeat("stopping", True)
            stop_children(active, args.shutdown_timeout)
            active.clear()
            completed, foreign_running, invalid, processed = inventory(plan)
            if invalid:
                failures.extend(invalid)
        if failures:
            heartbeat("failed", True)
            raise RuntimeError(f"worker failed; saved checkpoints are retained. See {failures}")
        if received_signals:
            heartbeat("stopped", True)
            return 128 + received_signals[0]
        all_complete = len(completed) == plan.data["chunk_count"]
        if all_complete and not args.no_merge:
            heartbeat("merging", True)
            try:
                target = merge(plan, args.merge_profiles, cancelled=lambda: bool(received_signals))
            except MergeInterrupted:
                heartbeat("stopped", True)
                return 128 + received_signals[0]
            print(f"Merged results: {target}", flush=True)
        if received_signals:
            heartbeat("stopped", True)
            return 128 + received_signals[0]
        heartbeat("complete" if all_complete and not args.no_merge else
                  "chunks_complete" if all_complete else "stopped", True)
        return 0
    except Exception as error:
        failures.append(dict(error=str(error)))
        try:
            heartbeat("failed", True)
        except (OSError, ValueError):
            pass
        raise
    finally:
        # Includes launch/IO errors. Children receive SIGTERM before the root lock
        # is released; their own checkpoints remain the authoritative state.
        if active:
            stop_children(active, args.shutdown_timeout)
            active.clear()
        for signum, handler in old_handlers.items():
            signal.signal(signum, handler)


def add_creation_options(p):
    p.add_argument("--firms", type=int)
    p.add_argument("--workers", type=int)
    p.add_argument("--capacities")
    p.add_argument("--forbidden-pairs", type=int)
    p.add_argument("--binary", type=Path, help="compiled run_rda_exhaustive (engine is its sibling)")
    p.add_argument("--chunk-size", type=int, help="representatives per chunk; new-plan default 100000000")
    p.add_argument("--start-id", type=int)
    p.add_argument("--count", type=int)
    p.add_argument("--write-profiles", action="store_true", default=None)
    p.add_argument("--progress-every", type=int, help="checkpoint interval; new-plan default 100000")
    p.add_argument("--plan-limit", type=int, default=100000, help="maximum manifest rows when creating a plan")


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    commands = p.add_subparsers(dest="command", required=True)
    create = commands.add_parser("plan", help="write an immutable range manifest without executing RDA")
    add_creation_options(create)
    create.add_argument("--output", type=Path, required=True)
    create.add_argument("--dry-run", action="store_true")
    run = commands.add_parser("run", help="create or resume a plan, fill execution slots, and merge")
    add_creation_options(run)
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--jobs", type=int, default=32)
    run.add_argument("--status-every", type=float, default=10)
    run.add_argument("--shutdown-timeout", type=float, default=60)
    run.add_argument("--stop-after-chunks", type=int, help="finish at most N new chunk attempts, then stop")
    run.add_argument("--no-merge", action="store_true")
    run.add_argument("--merge-profiles", action="store_true")
    show = commands.add_parser("status", help="inspect actual chunk progress")
    show.add_argument("--output", type=Path, required=True)
    show.add_argument("--json", action="store_true")
    combine = commands.add_parser("merge", help="verify all planned ranges and merge their checkpoints")
    combine.add_argument("--output", type=Path, required=True)
    combine.add_argument("--merge-profiles", action="store_true")
    return p


def main(argv=None):
    args = parser().parse_args(argv)
    if args.command == "plan":
        build_plan(args)
        return 0
    if args.command == "run":
        if args.jobs < 1 or not math.isfinite(args.status_every) or args.status_every <= 0 \
                or not math.isfinite(args.shutdown_timeout) or args.shutdown_timeout <= 0 \
                or (args.progress_every is not None and args.progress_every < 1) \
                or (args.stop_after_chunks is not None and args.stop_after_chunks < 1):
            raise ValueError("jobs, status-every, shutdown-timeout, progress-every and stop-after-chunks must be positive")
        if args.output.exists():
            plan = Plan.load(args.output)
            assert_existing_options(args, plan)
        else:
            if args.merge_profiles and not args.write_profiles:
                raise ValueError("--merge-profiles requires --write-profiles in the new plan")
            plan = build_plan(args)
        if args.merge_profiles and not plan.config["write_profiles"]:
            raise ValueError("--merge-profiles requires --write-profiles in the plan")
        with manager_lock(plan):
            return supervise(plan, args)
    plan = Plan.load(args.output)
    if args.command == "status":
        current = status(plan)
        if args.json:
            print(runner.encode_json(current))
        else:
            print(f'Status: {current["status"]}\n'
                  f'Chunks: {current["completed_chunks"]}/{current["total_chunks"]} complete\n'
                  f'Running chunk IDs: {current["running_chunks"]}\n'
                  f'Representatives: {current["processed_representative_inputs"]}/'
                  f'{current["selected_representative_inputs"]}\n'
                  f'Invalid chunks: {current["invalid_chunks"]}\n'
                  f'Merged results complete: {current["merged_complete"]}')
        return 1 if current["invalid_chunks"] or current["status"] == "failed" else 0
    with manager_lock(plan):
        print(f"Merged results: {merge(plan, args.merge_profiles)}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, RuntimeError, KeyError, TypeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
