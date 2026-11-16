"""Exhaustive explicit slices of preference space, all forbidden graphs per row."""
import argparse
import hashlib
import itertools
import json
import time

from search_symbolic import Symbolic,rankings_problem,key_of,graph_problem,direct_good,COUNT


def profiles(seed,mode):
    if mode=='single_firm_orders':
        seen=set()
        for firm in range(3):
            for order in itertools.permutations(seed['firm_rankings_one_based'][firm]):
                rows=[r.copy() for r in seed['firm_rankings_one_based']];rows[firm]=list(order)
                p=rankings_problem(seed['worker_rankings_one_based'],rows)
                key=key_of(p)
                if key in seen:continue
                seen.add(key)
                yield p,dict(firm=firm+1,ranking=list(order))
    elif mode=='worker_fixed_masks':
        choices=[list(itertools.permutations(row)) for row in seed['worker_rankings_one_based']]
        for orders in itertools.product(*choices):
            rows=[list(r) for r in orders]
            yield rankings_problem(rows,seed['firm_rankings_one_based']),dict(worker_orders=rows)


def main(args):
    raw=json.loads(args.seed.read_text())
    seed=rankings_problem(raw['worker_rankings_one_based'],raw['firm_rankings_one_based'])
    args.output.mkdir(parents=True,exist_ok=True)
    checked=positive=0;found=False;started=time.monotonic()
    with (args.output/'profile_scan.jsonl').open('w') as log:
        for p,description in profiles(seed,args.mode):
            symbolic=Symbolic(p);good=symbolic.good(0,2);checked+=1
            target=symbolic.best_target(good) if good else None
            record=dict(index=checked,description=description,
                        preference_sha256=hashlib.sha256(repr(key_of(p)).encode()).hexdigest(),
                        all_fail_depth_two_graphs=good.bit_count(),interference_target=target)
            log.write(json.dumps(record)+'\n')
            if good:
                positive+=1;graph=target['graph'] if target else (good&-good).bit_length()-1
                witness=dict(problem=graph_problem(p,graph),graph=graph,target=target,description=description)
                (args.output/'best_witness.json').write_text(json.dumps(witness,indent=2)+'\n')
                if target and target['exact_restoration']:
                    assert direct_good(p,graph);found=True;print('FOUND',json.dumps(record),flush=True);break
            if checked%1000==0:
                log.flush();print(f'profiles={checked} positive={positive} elapsed={time.monotonic()-started:.1f}s',flush=True)
    summary=dict(mode=args.mode,preference_space_exhaustive=False,specified_slice_exhausted=not found,
                 profiles_checked=checked,forbidden_graphs_per_profile=COUNT,
                 profiles_with_depth_two_all_fail=positive,exact_interference_example_found=found,
                 elapsed_seconds=time.monotonic()-started)
    (args.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2),flush=True)


if __name__=='__main__':
    from pathlib import Path
    parser=argparse.ArgumentParser()
    parser.add_argument('--seed',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--mode',choices=['single_firm_orders','worker_fixed_masks'],required=True)
    main(parser.parse_args())
