"""Check the exact partial-order family for the main example (14,400 orders)."""
import itertools
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent))
from render_da_search_trees import Model, blocker_pairs
from build_evidence import rankings_problem


def main():
    workers = [[2, 1]] * 4 + [[1, 2]]
    expected = [[1, 1, 0, 2, 2], [0, 1, 2, 2, 1],
                [0, 2, 1, 2, 1], [1, 2, 0, 2, 1]]
    deletion_sets = [set(), {(1, 1)}, {(1, 1), (2, 3)}, {(2, 3)}]
    successful = []
    shapes = Counter()
    checked = 0
    for left in itertools.permutations(range(1, 6)):
        for right in itertools.permutations(range(1, 6)):
            checked += 1
            # f1: {w1,w2} > w5 > w3; w4 anywhere.
            # f2: {w4,w5} > w3 > w2 > w1.
            pos1, pos2 = ({w:i for i,w in enumerate(row)} for row in (left,right))
            predicted = (max(pos1[1], pos1[2]) < pos1[5] < pos1[3] and
                         max(pos2[4], pos2[5]) < pos2[3] < pos2[2] < pos2[1])
            p = rankings_problem(workers, [list(left), list(right)], [[1, 2], [3, 4]])
            m = Model(p)
            states = [m.full_da(frozenset(d)) for d in deletion_sets]
            actual = [z['assignment'] for z in states] == expected
            assert predicted == actual, (left, right, predicted, actual)
            if not actual:
                continue
            assert blocker_pairs(m, states[2]['assignment'], set()) == [{'firm': 1, 'worker': 1}]
            assert not blocker_pairs(m, states[3]['assignment'], set())
            paths = m.enumerate()
            shape = (len(paths), sum(z['stable'] for z in paths), max(len(z['trace'])-1 for z in paths))
            shapes[shape] += 1
            successful.append([left, right])
    assert len(successful) == 20
    report = dict(checked=checked, successful=len(successful),
                  partial_order_is_necessary_and_sufficient_for_all_four_assignments=True,
                  firm1_constraints=['w1 > w5', 'w2 > w5', 'w5 > w3'],
                  firm2_constraints=['w4 > w3', 'w5 > w3', 'w3 > w2', 'w2 > w1'],
                  successful_orders=successful,
                  tree_shapes=[dict(leaves=k[0],stable_leaves=k[1],maximum_depth=k[2],count=v)
                               for k,v in sorted(shapes.items())])
    (ROOT / 'family_verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k:v for k,v in report.items() if k!='successful_orders'}, indent=2))


if __name__ == '__main__':
    main()
