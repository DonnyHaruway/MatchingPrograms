"""Independent exhaustive coverage and stable-path certificate verification.

DA uses simultaneous proposal rounds. Qualifying forbidden graphs are derived
as 1024-bit Boolean truth tables, not by the C++ graph loop or tree traversal.
Every qualifying input must have exactly one certificate, and every certified
deletion must resolve a currently co-located forbidden pair. The final matching
must be original-market UR-stable. This proves an existential stable branch
for every qualifying input without trusting the C++ all-branches search.
"""
import argparse
import gzip
import itertools
import json
import lzma
import struct
import time
from pathlib import Path

W = 5
TYPES = ((2, 1), (1, 2), (1, -1), (-1, 1), (-1, -1))
PAIRS = list(itertools.combinations(range(W), 2))
EDGE = {frozenset(pair): 1 << k for k, pair in enumerate(PAIRS)}
ALL = (1 << 1024) - 1
HAS = [sum(1 << graph for graph in range(1024) if graph & (1 << k))
       for k in range(10)]
ANY, EVERY = [0] * 1024, [ALL] * 1024
for mask in range(1, 1024):
    low = mask & -mask
    k = low.bit_length() - 1
    ANY[mask] = ANY[mask ^ low] | HAS[k]
    EVERY[mask] = EVERY[mask ^ low] & HAS[k]


def edge(a, b):
    return EDGE[frozenset((a, b))]


def records(path):
    opener = lzma.open if path.suffix == '.xz' else gzip.open if path.suffix == '.gz' else open
    record = struct.Struct('<IHB5s')
    with opener(path, 'rb') as source:
        assert source.read(4) == b'URC1', 'certificate header'
        while True:
            block = source.read(record.size * 65536)
            if not block:
                break
            assert len(block) % record.size == 0, 'truncated certificate'
            for profile, graph, length, packed in record.iter_unpack(block):
                yield profile, graph, length, int.from_bytes(packed, 'little')


class BatchDA:
    def __init__(self, worker, firm):
        self.worker, self.firm = worker, firm
        self.orders = [sorted((h for h in range(2) if row[h] > 0), key=lambda h: -row[h])
                       for row in worker]
        self.cache = {}
        self.blocker_cache = {}

    def state(self, deleted):
        if deleted in self.cache:
            return self.cache[deleted]
        assignment = [-1] * W
        cursor = [0] * W
        teams = [[], []]
        while True:
            proposals = [[], []]
            active = False
            for w in range(W):
                if assignment[w] != -1 or cursor[w] == len(self.orders[w]):
                    continue
                active = True
                h = self.orders[w][cursor[w]]
                cursor[w] += 1
                if not deleted & (1 << (h * W + w)):
                    proposals[h].append(w)
            if not active:
                break
            for h in range(2):
                available = teams[h] + proposals[h]
                teams[h] = sorted(available, key=lambda w: -self.firm[h][w])[:2]
                for w in available:
                    assignment[w] = h if w in teams[h] else -1
        result = tuple(assignment), tuple(tuple(team) for team in teams)
        self.cache[deleted] = result
        return result

    def ordinary(self, result, h, w):
        assignment, teams = result
        if assignment[w] == h:
            return False
        current = 0 if assignment[w] == -1 else self.worker[w][assignment[w]]
        return self.worker[w][h] > current and (len(teams[h]) < 2 or
                any(self.firm[h][w] > self.firm[h][v] for v in teams[h]))

    @staticmethod
    def candidate_edges(result, h, w):
        mask = 0
        for v in result[1][h]:
            mask |= edge(v, w)
        return mask

    @staticmethod
    def inside(result):
        mask = 0
        for team in result[1]:
            if len(team) == 2:
                mask |= edge(*team)
        return mask

    def blocker_masks(self, deleted):
        if deleted not in self.blocker_cache:
            z = self.state(deleted)
            self.blocker_cache[deleted] = [self.candidate_edges(z, h, w)
                for h in range(2) for w in range(W) if self.ordinary(z, h, w)]
        return self.blocker_cache[deleted]

    def path_family(self, length, packed):
        assert 0 <= length <= 10 and packed >> (4 * length) == 0
        required, deleted = 0, 0
        for i in range(length):
            contract = (packed >> (4 * i)) & 15
            assert contract < 10 and not deleted & (1 << contract)
            h, w = divmod(contract, W)
            assignment, teams = self.state(deleted)
            assert assignment[w] == h and len(teams[h]) == 2
            other = next(v for v in teams[h] if v != w)
            required |= edge(w, other)
            deleted |= 1 << contract
        leaf = self.state(deleted)
        valid = EVERY[required] & ~ANY[self.inside(leaf)]
        for incompatibility in self.blocker_masks(deleted):
            valid &= ANY[incompatibility]
        return valid

    def validate_path(self, length, packed, graphs):
        valid = self.path_family(length, packed)
        assert not graphs & ~valid, 'certificate is not an allowed path to a stable leaf'


def verify(mode, certificate, compare):
    started = time.monotonic()
    base = 2 if mode == 'complete' else 5
    total = base ** W
    iterator = records(certificate)
    current = next(iterator, None)
    counts = dict(profiles=0, first_paths=0, second_paths=0,
                  ordinary_ancestor_blocker_paths=0, ancestor_ur_paths=0,
                  admissible_graph_path_combinations=0,
                  new_ancestor_graph_path_combinations=0,
                  obstructed_graph_path_combinations=0,
                  exact_graph_path_combinations=0,
                  unique_qualifying_profile_graph_inputs=0)
    verified_records = verified_paths = 0
    for perm_index, perm in enumerate(itertools.permutations(range(W))):
        firm = (tuple(range(W, 0, -1)), tuple(W - perm.index(w) for w in range(W)))
        for code in range(total):
            profile = perm_index * total + code
            n = code
            worker = []
            for _ in range(W):
                worker.append(TYPES[n % base])
                n //= base
            model = BatchDA(worker, firm)
            counts['profiles'] += 1
            qualifying = 0
            a = model.state(0)
            for f in range(2):
                for r, s in itertools.permutations(a[1][f], 2):
                    counts['first_paths'] += 1
                    d1 = 1 << (f * W + r)
                    b = model.state(d1)
                    for g in range(2):
                        for t, u in itertools.permutations(b[1][g], 2):
                            counts['second_paths'] += 1
                            d2 = 1 << (g * W + t)
                            c = model.state(d1 | d2)
                            if not model.ordinary(c, f, r):
                                continue
                            counts['ordinary_ancestor_blocker_paths'] += 1
                            required = edge(r, s) | edge(t, u)
                            invalid = model.inside(c) | model.candidate_edges(c, f, r)
                            if required & invalid:
                                continue
                            counts['ancestor_ur_paths'] += 1
                            graphs = EVERY[required] & ~ANY[invalid]
                            counts['admissible_graph_path_combinations'] += graphs.bit_count()
                            if model.ordinary(c, g, t):
                                graphs &= ANY[model.candidate_edges(c, g, t)]
                            if model.ordinary(b, f, r):
                                graphs &= ANY[model.candidate_edges(b, f, r)]
                            counts['new_ancestor_graph_path_combinations'] += graphs.bit_count()
                            qualifying |= graphs
                            if not graphs:
                                continue
                            restored = model.state(d2)
                            if restored[0][r] != f or restored[0][s] != f:
                                counts['obstructed_graph_path_combinations'] += graphs.bit_count()
                            if restored[0][r] == f and restored[0][s] != f:
                                exact = graphs & ~ANY[model.inside(restored)]
                                if model.ordinary(restored, g, t):
                                    exact &= ANY[model.candidate_edges(restored, g, t)]
                                counts['exact_graph_path_combinations'] += exact.bit_count()
            counts['unique_qualifying_profile_graph_inputs'] += qualifying.bit_count()
            seen, paths = 0, {}
            assert current is None or current[0] >= profile, 'certificate profile ordering'
            while current is not None and current[0] == profile:
                _, graph, length, packed = current
                assert 0 <= graph < 1024
                bit = 1 << graph
                assert not seen & bit, 'duplicate certificate input'
                seen |= bit
                key = length, packed
                paths[key] = paths.get(key, 0) | bit
                verified_records += 1
                current = next(iterator, None)
            assert seen == qualifying, ('coverage mismatch', profile,
                                         (seen ^ qualifying).bit_count())
            for (length, packed), graphs in paths.items():
                model.validate_path(length, packed, graphs)
                verified_paths += 1
        if (perm_index + 1) % 10 == 0:
            print(f'permutations={perm_index+1} profiles={counts["profiles"]} '
                  f'verified_inputs={verified_records} elapsed={time.monotonic()-started:.1f}s', flush=True)
    assert current is None, 'unexpected trailing certificate profiles'
    cpp = json.loads(compare.read_text())
    differences = {key: (value, cpp.get(key)) for key, value in counts.items()
                   if value != cpp.get(key)}
    assert not differences, differences
    assert verified_records == cpp['stable_path_certificates']
    assert cpp['all_branches_fail_inputs'] == 0
    return dict(mode=mode, counts=counts, certificate_records_verified=verified_records,
                distinct_stable_path_groups_verified=verified_paths,
                all_qualifying_inputs_covered=True,
                every_qualifying_input_has_an_independently_verified_stable_branch=True,
                cpp_counts_match=True, all_branches_fail_inputs=0,
                elapsed_seconds=time.monotonic()-started)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=['complete', 'incomplete'], required=True)
    parser.add_argument('--certificate', type=Path, required=True)
    parser.add_argument('--compare', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.mode, args.certificate, args.compare)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
