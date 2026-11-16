"""Check focused witnesses using the pre-existing independent FIFO DA model."""
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent))
from render_da_search_trees import Model, blocker_pairs


def problem_from_witness(x):
    n = len(x['worker_scores'])
    cp = [[0] * n for _ in range(n)]
    pairs = []
    bit = 0
    for a in range(n):
        for b in range(a + 1, n):
            if x['forbidden_mask'] & (1 << bit):
                pairs.append([a + 1, b + 1])
                cp[a][b] = cp[b][a] = -100000
            bit += 1
    p = dict(n_workers=n, n_firms=2, capacities=x['capacities'],
             agent_prefs=x['worker_scores'], firm_prefs=x['firm_scores'],
             colleague_prefs=cp, forbidden_pairs_one_based=pairs)
    for key, dest in [('agent_prefs', 'worker_rankings_one_based'),
                      ('firm_prefs', 'firm_rankings_one_based')]:
        p[dest] = [[i + 1 for i in sorted(range(len(row)), key=lambda i: -row[i])
                    if row[i] > 0] for row in p[key]]
    return p


def check(x):
    p = problem_from_witness(x)
    m = Model(p)
    f, r, s = x['first']
    g, t, u = x['second']
    d1, d2 = (f, r), (g, t)
    states = [m.full_da(frozenset(d)) for d in (set(), {d1}, {d1, d2}, {d2})]
    assert [z['assignment'] for z in states[:3]] == x['assignments']
    assert any(c['firm'] == f and {c['left'], c['right']} == {r, s}
               for c in states[0]['conflicts'])
    assert any(c['firm'] == g and {c['left'], c['right']} == {t, u}
               for c in states[1]['conflicts'])
    assert not states[2]['conflicts'] and not states[3]['conflicts']
    assert blocker_pairs(m, states[2]['assignment'], set()) == [{'firm': f, 'worker': r}]
    assert not blocker_pairs(m, states[1]['assignment'], set())
    assert not blocker_pairs(m, states[3]['assignment'], set())
    assert states[3]['assignment'][r - 1] == f
    assert states[3]['assignment'][s - 1] != f
    paths = m.enumerate()
    target = next(z for z in paths if [(a['selected_pair']['firm'], a['removed_worker'])
                  for a in z['trace'][:-1]] == [d1, d2])
    assert not target['stable']
    return p, paths, states


def main():
    report = {}
    for mode in ('complete', 'incomplete'):
        histogram = collections.Counter()
        ranked = []
        for line in (ROOT / f'{mode}_focused_witnesses.jsonl').read_text().splitlines():
            x = json.loads(line)
            p, paths, states = check(x)
            depths = [len(z['trace']) - 1 for z in paths]
            stable = sum(z['stable'] for z in paths)
            shape = (len(paths), stable, min(depths), max(depths))
            histogram[shape] += 1
            # Favor a depth-two whole tree with no original-stable leaves, and
            # then a one-deletion sibling terminal, as in the reference tree.
            score = (max(depths) == 2, stable == 0, min(depths) == 1,
                     -len(p['forbidden_pairs_one_based']),
                     -sum(len(z['proposals']) for z in states), -len(paths))
            ranked.append((score, x, shape))
        ranked.sort(key=lambda v: v[0], reverse=True)
        report[mode] = dict(checked=len(ranked), histogram=[
            dict(leaves=k[0], stable_leaves=k[1], min_depth=k[2], max_depth=k[3], count=v)
            for k, v in sorted(histogram.items())],
            best=[dict(witness=x, shape=shape) for _, x, shape in ranked[:10]])
    (ROOT / 'candidate_analysis.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({mode: {k:v for k,v in data.items() if k!='best'}
                      for mode,data in report.items()}, indent=2))


if __name__ == '__main__':
    main()
