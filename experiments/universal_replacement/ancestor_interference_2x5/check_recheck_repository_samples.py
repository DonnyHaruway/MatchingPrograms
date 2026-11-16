"""Cross-check selected full trees against the unmodified repository C++ API."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from analyze_candidates import problem_from_witness
from verify_all_graph_certificates import TYPES
from render_da_search_trees import Model, verify

ROOT = Path(__file__).resolve().parent


def main():
    bridge = sys.argv[1]
    configurations = [
        ([0, 0, 0, 0, 0], [0, 1, 2, 3, 4]),
        ([0, 1, 0, 1, 0], [4, 3, 2, 1, 0]),
        ([0, 2, 1, 3, 4], [2, 4, 1, 0, 3]),
        ([4, 3, 2, 1, 0], [1, 3, 0, 4, 2]),
    ]
    reports = []
    with tempfile.TemporaryDirectory(prefix='ancestor-repository-samples-', dir='/private/tmp') as temp:
        target = Path(temp) / 'tree.json'
        for types, order in configurations:
            for graph in (0, 514, 1023):
                scores = [5 - order.index(w) for w in range(5)]
                p = problem_from_witness(dict(worker_scores=[TYPES[t] for t in types],
                    firm_scores=[[5, 4, 3, 2, 1], scores], capacities=[2, 2], forbidden_mask=graph))
                text = '5 2\n2 2\n'
                for key in ('agent_prefs', 'firm_prefs', 'colleague_prefs'):
                    text += ''.join(' '.join(map(str, row)) + '\n' for row in p[key])
                subprocess.run([bridge, str(target)], input=text, text=True, check=True)
                tree = json.loads(target.read_text())
                result = verify(tree, Model(p))
                result.update(worker_types=types, firm2_order_zero_based=order,
                              forbidden_mask=graph,
                              maximum_deletion_depth=max(len(n['deletions']) for n in tree['nodes']))
                assert result['original_ur_stable_leaves'] > 0
                reports.append(result)
    out = dict(samples=len(reports), all_existing_cpp_and_python_full_trees_match=True,
               verified_nodes=sum(z['nodes'] for z in reports),
               verified_leaves=sum(z['leaves'] for z in reports), cases=reports)
    (ROOT / 'all_graphs_recheck/repository_sample_verification.json').write_text(json.dumps(out, indent=2)+'\n')
    print(json.dumps({k:v for k,v in out.items() if k!='cases'}, indent=2))


if __name__ == '__main__':
    main()
