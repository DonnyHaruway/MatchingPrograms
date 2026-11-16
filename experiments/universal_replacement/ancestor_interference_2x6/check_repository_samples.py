"""Check complete sparse-conflict trees with unchanged repository C++ and Python."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent))
from render_da_search_trees import Model,verify

TYPES=((2,1),(1,2),(1,-1),(-1,1),(-1,-1))


def problem(types,order,graph):
    cp=[[0]*6 for _ in range(6)];bit=0
    for a in range(6):
        for b in range(a+1,6):
            if graph&(1<<bit):cp[a][b]=cp[b][a]=-100000
            bit+=1
    wp=[list(TYPES[t]) for t in types]
    fp=[list(range(6,0,-1)),[6-order.index(w) for w in range(6)]]
    p=dict(n_workers=6,n_firms=2,capacities=[2,2],agent_prefs=wp,firm_prefs=fp,colleague_prefs=cp)
    for key,dest in [('agent_prefs','worker_rankings_one_based'),('firm_prefs','firm_rankings_one_based')]:
        p[dest]=[[i+1 for i in sorted(range(len(row)),key=lambda i:-row[i]) if row[i]>0] for row in p[key]]
    return p


def main():
    bridge=sys.argv[1]
    cases=[([0]*6,[0,1,2,3,4,5]),([0,1,0,1,0,1],[5,4,3,2,1,0]),
           ([0,2,1,3,4,0],[2,4,1,5,0,3]),([4,3,2,1,0,2],[1,3,5,0,4,2])]
    reports=[]
    with tempfile.TemporaryDirectory(prefix='ancestor-2x6-core-',dir='/private/tmp') as temp:
        target=Path(temp)/'tree.json'
        for types,order in cases:
            for graph in (0,513,545):
                p=problem(types,order,graph)
                text='6 2\n2 2\n'
                for key in ('agent_prefs','firm_prefs','colleague_prefs'):
                    text+=''.join(' '.join(map(str,row))+'\n' for row in p[key])
                subprocess.run([bridge,str(target)],input=text,text=True,check=True)
                tree=json.loads(target.read_text());report=verify(tree,Model(p))
                report.update(worker_types=types,firm2_order_zero_based=order,forbidden_mask=graph,
                              max_depth=max(len(n['deletions']) for n in tree['nodes']))
                assert report['original_ur_stable_leaves']>0
                reports.append(report)
    result=dict(samples=len(reports),verified_nodes=sum(z['nodes'] for z in reports),
                verified_leaves=sum(z['leaves'] for z in reports),
                all_repository_cpp_and_python_trees_match=True,cases=reports)
    (ROOT/'results/repository_sample_verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='cases'},indent=2))


if __name__=='__main__':main()
