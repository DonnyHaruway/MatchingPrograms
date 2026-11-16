#!/usr/bin/env python3
"""Exact input counts for the four agreed markets; does not run RDA.

All counterpart labels and the outside option occur in every strict order.
Capacities are fixed in the displayed order. Relabelings preserving that vector
form S_m x product_c S_{number of firms of capacity c}.

Each market has a uniquely capacitated firm. An automorphism must fix this
firm, whose strict order then fixes every worker. A worker's strict order in
turn fixes every firm. Thus the action is free: every orbit has group_size
members, even after restricting the number of forbidden undirected edges.
"""

import argparse
from collections import Counter
from itertools import product
import json
from math import comb, factorial, prod


MARKETS = ((2, 2, (1, 2)), (2, 3, (1, 2)),
           (3, 4, (1, 1, 2)), (3, 5, (1, 2, 2)))


def estimate(n, m, capacities):
    multiplicities = Counter(capacities)
    assert len(capacities) == n
    assert 1 in multiplicities.values(), "Free-action proof needs a unique capacity"
    firm_symmetries = prod(factorial(v) for v in multiplicities.values())
    group_size = factorial(m) * firm_symmetries
    preferences = factorial(n + 1) ** m * factorial(m + 1) ** n
    pairs = comb(m, 2)
    by_k = []
    for k in range(1, m):
        graphs = comb(pairs, k)
        labeled = preferences * graphs
        assert labeled % group_size == 0
        by_k.append(dict(k=k, forbidden_sets=graphs,
                         labeled_inputs=labeled,
                         representative_inputs=labeled // group_size))

    # Count all capacity-respecting assignments, including unmatched workers.
    # Independent multinomial and direct-enumeration calculations must agree.
    assignments = sum(
        factorial(m) // (factorial(m - sum(sizes)) * prod(factorial(s) for s in sizes))
        for sizes in product(*(range(c + 1) for c in capacities))
        if sum(sizes) <= m
    )
    direct_assignments = sum(
        all(a.count(f + 1) <= c for f, c in enumerate(capacities))
        for a in product(range(n + 1), repeat=m)
    )
    assert assignments == direct_assignments
    representatives = sum(row["representative_inputs"] for row in by_k)
    return dict(
        n=n, m=m, capacities=list(capacities), orbit_weight=group_size,
        by_k=by_k, labeled_inputs=representatives * group_size,
        representative_inputs=representatives,
        assignments_per_input_before_forbidden_filter=assignments,
        assignment_candidates_before_forbidden_filter=representatives * assignments,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Print exact counts as JSON")
    args = parser.parse_args()
    markets = [estimate(*market) for market in MARKETS]
    if args.json:
        print(json.dumps({"markets": markets}, ensure_ascii=False, indent=2))
        return
    print("n,m,capacities,k,forbidden_sets,labeled_inputs,representative_inputs,orbit_weight")
    for market in markets:
        for row in market["by_k"]:
            print(f'{market["n"]},{market["m"]},"{market["capacities"]}",'
                  f'{row["k"]},{row["forbidden_sets"]},{row["labeled_inputs"]},'
                  f'{row["representative_inputs"]},{market["orbit_weight"]}')


if __name__ == "__main__":
    main()
