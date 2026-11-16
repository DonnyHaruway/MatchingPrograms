"""Independent enumeration: batch-round DA and monotone forbidden-graph constraints.

This does not import the fast C++ model or use its matching outputs.
All IDs in the internal model are zero-based, unmatched = -1.
"""
import argparse
import itertools
import json
import time
from pathlib import Path

TYPES = ((2, 1), (1, 2), (1, -1), (-1, 1), (-1, -1))


def search(n, complete):
    pairs = list(itertools.combinations(range(n), 2))
    pair_bits = {frozenset(pair): 1 << i for i, pair in enumerate(pairs)}
    universe = (1 << len(pairs)) - 1
    counts = dict(profiles=0, first_paths=0, second_paths=0,
                  ordinary_ancestor_blocker_paths=0, ancestor_ur_paths=0,
                  clean_ancestor_ur_paths=0, clean_same_pair_paths=0,
                  new_ancestor_ur_paths=0, new_full_paths=0,
                  new_full_cross_firm_paths=0, new_introduced_conflict_paths=0,
                  new_repeated_worker_paths=0, exact_restoration_paths=0,
                  exact_full_paths=0, exact_cross_firm_paths=0,
                  exact_repeated_worker_paths=0, unmatched_contract_deletions=0,
                  unmatched_contract_changes=0)

    def edge(a, b):
        return pair_bits[frozenset((a, b))]

    def inside(teams):
        mask = 0
        for team in teams:
            for a, b in itertools.combinations(team, 2):
                mask |= edge(a, b)
        return mask

    started = time.monotonic()
    for perm in itertools.permutations(range(n)):
        firm = [tuple(range(n, 0, -1)), tuple(n - perm.index(w) for w in range(n))]
        for worker in itertools.product(TYPES[:2] if complete else TYPES, repeat=n):
            counts['profiles'] += 1
            orders = [sorted((h for h in range(2) if row[h] > 0), key=lambda h: -row[h])
                      for row in worker]

            def da(deleted):
                # All currently unmatched workers propose together in each round.
                assignment = [-1] * n
                cursor = [0] * n
                teams = [[], []]
                while True:
                    proposals = [[], []]
                    for w in range(n):
                        if assignment[w] != -1 or cursor[w] == len(orders[w]):
                            continue
                        h = orders[w][cursor[w]]
                        cursor[w] += 1
                        if (h, w) not in deleted:
                            proposals[h].append(w)
                    if not any(proposals):
                        if any(assignment[w] == -1 and cursor[w] < len(orders[w])
                               for w in range(n)):
                            continue
                        return tuple(assignment), teams
                    for h in range(2):
                        available = teams[h] + proposals[h]
                        teams[h] = sorted(available, key=lambda w: -firm[h][w])[:2]
                        for w in available:
                            assignment[w] = h if w in teams[h] else -1

            def ordinary(result, h, w):
                assignment, teams = result
                if assignment[w] == h:
                    return False
                current = 0 if assignment[w] == -1 else worker[w][assignment[w]]
                return worker[w][h] > current and (len(teams[h]) < 2 or
                        any(firm[h][w] > firm[h][v] for v in teams[h]))

            def candidate_edges(result, h, w):
                mask = 0
                for v in result[1][h]:
                    if v != w:
                        mask |= edge(v, w)
                return mask

            def ur(result, graph, h, w):
                return ordinary(result, h, w) and not (candidate_edges(result, h, w) & graph)

            a = da(set())
            for w in range(n):
                for h in range(2):
                    if a[0][w] != h and worker[w][h] > 0:
                        counts['unmatched_contract_deletions'] += 1
                        counts['unmatched_contract_changes'] += da({(h, w)})[0] != a[0]
            for f in range(2):
                for r, s in itertools.permutations(a[1][f], 2):
                    counts['first_paths'] += 1
                    d1 = (f, r)
                    b = da({d1})
                    for g in range(2):
                        for t, u in itertools.permutations(b[1][g], 2):
                            counts['second_paths'] += 1
                            d2 = (g, t)
                            c = da({d1, d2})
                            if not ordinary(c, f, r):
                                continue
                            counts['ordinary_ancestor_blocker_paths'] += 1
                            required = edge(r, s) | edge(t, u)
                            invalid = inside(c[1]) | candidate_edges(c, f, r)
                            if required & invalid:
                                continue
                            counts['ancestor_ur_paths'] += 1
                            # More forbidden edges can only destroy a blocking pair.
                            # Under the negative constraints, the largest graph is
                            # therefore a complete existential witness for all
                            # simultaneous "not a blocker" conditions below.
                            graph = universe ^ invalid
                            if ur(c, graph, g, t):
                                continue
                            counts['clean_ancestor_ur_paths'] += 1
                            counts['clean_same_pair_paths'] += edge(r, s) == edge(t, u)
                            if ur(b, graph, f, r):
                                continue
                            counts['new_ancestor_ur_paths'] += 1
                            full = all(len(team) == 2 for team in c[1])
                            counts['new_full_paths'] += full
                            counts['new_full_cross_firm_paths'] += full and f != g
                            counts['new_introduced_conflict_paths'] += a[0][t] != g or a[0][u] != g
                            counts['new_repeated_worker_paths'] += r == t
                            restored = da({d2})
                            invalid_exact = invalid | inside(restored[1])
                            if required & invalid_exact:
                                continue
                            graph = universe ^ invalid_exact
                            if (restored[0][r] == f and restored[0][s] != f and
                                not ur(b, graph, f, r) and not ur(c, graph, g, t) and
                                not ur(restored, graph, g, t)):
                                counts['exact_restoration_paths'] += 1
                                counts['exact_full_paths'] += full
                                counts['exact_cross_firm_paths'] += f != g
                                counts['exact_repeated_worker_paths'] += r == t
        if n == 5 and counts['profiles'] % 31250 == 0:
            print(f'profiles={counts["profiles"]} elapsed={time.monotonic()-started:.1f}s', flush=True)
    return dict(n_workers=n, mode='complete' if complete else 'incomplete',
                counts=counts, elapsed_seconds=time.monotonic()-started)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--workers', type=int, default=4)
    parser.add_argument('--complete', action='store_true')
    parser.add_argument('--compare', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = search(args.workers, args.complete)
    if args.compare:
        other = json.loads(args.compare.read_text())
        differences = {key: (value, other.get(key)) for key, value in result['counts'].items()
                       if value != other.get(key)}
        if differences:
            raise AssertionError(differences)
        result['all_compared_cpp_counts_match'] = True
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
