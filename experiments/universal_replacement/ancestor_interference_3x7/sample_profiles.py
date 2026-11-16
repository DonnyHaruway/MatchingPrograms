"""Seeded, explicitly non-exhaustive profile samples; all graphs per sample."""
import argparse
import hashlib
import json
import random
import time
from pathlib import Path

from search_symbolic import Symbolic,rankings_problem,key_of,graph_problem,direct_good,neighbors,COUNT


def main(args):
    raw=json.loads(args.seed.read_text())
    seed=rankings_problem(raw['worker_rankings_one_based'],raw['firm_rankings_one_based'])
    rng=random.Random(args.random_seed)
    seen=set();checked=positive=0;best=None;counts={};started=time.monotonic()
    args.output.mkdir(parents=True,exist_ok=True)
    with (args.output/'profile_scan.jsonl').open('w') as log:
        while checked<args.profiles:
            category=('complete','seed_masks','partial','local_walk')[checked%4]
            if category=='local_walk':
                p=seed
                for _ in range(rng.randrange(4,16)):
                    p,_=rng.choice(list(neighbors(p)))
            else:
                if category=='seed_masks':
                    workers=[row.copy() for row in seed['worker_rankings_one_based']]
                    firms=[row.copy() for row in seed['firm_rankings_one_based']]
                else:
                    chance=1 if category=='complete' else .85
                    workers=[[f for f in range(1,4) if rng.random()<chance] for _ in range(7)]
                    firms=[[w for w in range(1,8) if rng.random()<chance] for _ in range(3)]
                for row in workers+firms:rng.shuffle(row)
                p=rankings_problem(workers,firms)
            key=key_of(p)
            if key in seen:continue
            seen.add(key);checked+=1;counts[category]=counts.get(category,0)+1
            symbolic=Symbolic(p);good=symbolic.good(0,args.max_tree_depth)
            target=symbolic.best_target(good) if good else None
            record=dict(index=checked,category=category,
                        preference_sha256=hashlib.sha256(repr(key).encode()).hexdigest(),
                        all_fail_bounded_depth_graphs=good.bit_count(),maximum_tree_depth=args.max_tree_depth,
                        interference_target=target)
            log.write(json.dumps(record)+'\n')
            if good:
                positive+=1;graph=target['graph'] if target else (good&-good).bit_length()-1
                witness=dict(problem=graph_problem(p,graph),graph=graph,category=category,target=target,
                             random_seed=args.random_seed,sample_index=checked)
                if best is None or (target and (not best['target'] or target['score']>best['target']['score'])):
                    best=witness;(args.output/'best_witness.json').write_text(json.dumps(witness,indent=2)+'\n')
                if target and target['exact_restoration']:
                    assert direct_good(p,graph,args.max_tree_depth)
                    print('FOUND',json.dumps(record),flush=True);break
            if checked%1000==0:
                log.flush();print(f'profiles={checked} positive={positive} elapsed={time.monotonic()-started:.1f}s',flush=True)
    summary=dict(preference_space_exhaustive=False,random_seed=args.random_seed,
                 requested_profiles=args.profiles,profiles_checked=checked,
                 forbidden_graphs_per_profile=COUNT,maximum_tree_depth=args.max_tree_depth,
                 profiles_with_bounded_depth_all_fail=positive,
                 by_category=counts,
                 exact_interference_example_found=bool(best and best['target'] and best['target']['exact_restoration']),
                 elapsed_seconds=time.monotonic()-started)
    (args.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--seed',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--profiles',type=int,default=20000)
    parser.add_argument('--random-seed',type=int,default=20261104)
    parser.add_argument('--max-tree-depth',type=int,default=2)
    main(parser.parse_args())
