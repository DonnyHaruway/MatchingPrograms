"""Audit saved inputs only; does not generate or search new preference profiles."""
import hashlib
import json
from pathlib import Path

from analyze_candidates import problem_from_witness
from render_da_search_trees import Model

ROOT = Path(__file__).resolve().parent


def main():
    markets, by_file, source_hashes = {}, {}, {}
    for path in sorted(ROOT.glob('*witnesses.jsonl')):
        source_hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        rows = []
        for line in path.read_text().splitlines():
            p = problem_from_witness(json.loads(line))
            key = json.dumps([p['agent_prefs'], p['firm_prefs'],
                              p['colleague_prefs'], p['capacities']])
            if key not in markets:
                paths = Model(p).enumerate()
                markets[key] = dict(leaves=len(paths),
                                    stable_leaves=sum(z['stable'] for z in paths),
                                    max_depth=max(len(z['trace']) - 1 for z in paths))
            rows.append(markets[key])
        by_file[path.name] = dict(saved_paths=len(rows),
                                  all_branches_fail=sum(z['stable_leaves'] == 0 for z in rows),
                                  depth_at_most_two_all_fail=sum(
                                      z['stable_leaves'] == 0 and z['max_depth'] <= 2 for z in rows))
    recorded = []
    for path in sorted((ROOT.parent / 'tree_docs/recorded_results').glob('*/recursive_doctor.json')):
        tree = json.loads(path.read_text())
        leaves = [tree['nodes'][i] for i in tree['leaves']]
        recorded.append(dict(profile=path.parent.name, leaves=len(leaves),
                             stable_leaves=sum(z['original_ur_stable'] for z in leaves),
                             max_depth=max(len(z['deletions']) for z in leaves)))
    result = dict(scope='Saved witnesses only; no new input or forbidden graph generation.',
                  unique_saved_2x5_inputs=len(markets),
                  all_branches_fail_inputs=sum(z['stable_leaves'] == 0 for z in markets.values()),
                  by_file=by_file, recorded_recursive_doctor=recorded,
                  source_sha256=source_hashes,
                  limitation='Previous search saves a representative forbidden graph per path, '
                             'rather than every qualifying graph. Zero saved examples is '
                             'not an impossibility proof for the new all-branches condition.')
    (ROOT / 'saved_all_branches_check.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k:v for k,v in result.items() if k!='source_sha256'}, indent=2))


if __name__ == '__main__':
    main()
