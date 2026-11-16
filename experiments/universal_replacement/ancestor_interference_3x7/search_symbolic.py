"""All forbidden graphs for each profile; search strict-depth-two failure trees.

Preferences are explored in explicit, recorded finite batches. This is NOT an
exhaustive enumeration of all possible 3x7 preference profiles.
"""
import argparse
import copy
import hashlib
import itertools
import json
import random
import sys
import time
from collections import deque
from functools import lru_cache
from pathlib import Path

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent))
from render_da_search_trees import Model,blocker_pairs

N,F=7,3
PAIRS=list(itertools.combinations(range(1,N+1),2))
EDGE={frozenset(pair):1<<i for i,pair in enumerate(PAIRS)}
COUNT=1<<len(PAIRS)
ALL=(1<<COUNT)-1


def truth_bit(bit):
    run=1<<bit
    value=((1<<run)-1)<<run
    width=2*run
    while width<COUNT:
        value|=value<<width
        width*=2
    return value


HAS=[truth_bit(i) for i in range(len(PAIRS))]


@lru_cache(maxsize=4096)
def any_edges(mask):
    result=0
    while mask:
        low=mask&-mask
        result|=HAS[low.bit_length()-1]
        mask^=low
    return result


def edge(a,b):return EDGE[frozenset((a,b))]


def rankings_problem(workers,firms):
    def scores(rows,n):
        return [[len(row)-row.index(x+1) if x+1 in row else -100000 for x in range(n)] for row in rows]
    return dict(n_workers=N,n_firms=F,capacities=[2]*F,
                agent_prefs=scores(workers,F),firm_prefs=scores(firms,N),
                colleague_prefs=[[0]*N for _ in range(N)],
                worker_rankings_one_based=workers,firm_rankings_one_based=firms)


def key_of(p):return tuple(tuple(row) for row in p['worker_rankings_one_based']+p['firm_rankings_one_based'])


def graph_problem(p,graph):
    p=copy.deepcopy(p)
    for bit,(a,b) in enumerate(PAIRS):
        if graph&(1<<bit):p['colleague_prefs'][a-1][b-1]=p['colleague_prefs'][b-1][a-1]=-100000
    p['forbidden_pairs_one_based']=[list(pair) for bit,pair in enumerate(PAIRS) if graph&(1<<bit)]
    return p


class Symbolic:
    def __init__(self,p):
        self.p=p
        self.model=Model(p)
        self.states={}
        self.goods={}
        self.bad_cache={}
        self.stable_cache={}
        self.blocker_cache={}

    def state(self,deleted):
        if deleted not in self.states:
            d={(i//N+1,i%N+1) for i in range(F*N) if deleted&(1<<i)}
            self.states[deleted]=self.model.full_da(frozenset(d))['assignment']
        return self.states[deleted]

    def teams(self,deleted):
        a=self.state(deleted)
        return [[w for w,h in enumerate(a,1) if h==f] for f in range(1,F+1)]

    def inside(self,deleted):
        mask=0
        for team in self.teams(deleted):
            if len(team)==2:mask|=edge(*team)
        return mask

    def blockers(self,deleted):
        if deleted in self.blocker_cache:return self.blocker_cache[deleted]
        assignment=self.state(deleted);teams=self.teams(deleted);result={}
        for f,team in enumerate(teams):
            for w,current in enumerate(assignment):
                if current==f+1:continue
                current_score=0 if not current else self.p['agent_prefs'][w][current-1]
                if self.p['agent_prefs'][w][f]<=current_score:continue
                score=self.p['firm_prefs'][f][w]
                if len(team)<2:
                    if score<=0:continue
                elif not any(score>self.p['firm_prefs'][f][v-1] for v in team):continue
                incompat=0
                for v in team:incompat|=edge(w+1,v)
                result[f*N+w]=incompat
        self.blocker_cache[deleted]=result
        return result

    def leaf_bad(self,deleted):
        if deleted not in self.bad_cache:
            result=0
            for mask in self.blockers(deleted).values():result|=ALL^any_edges(mask)
            self.bad_cache[deleted]=result&(ALL^any_edges(self.inside(deleted)))
        return self.bad_cache[deleted]

    def leaf_stable(self,deleted):
        if deleted not in self.stable_cache:
            result=ALL^any_edges(self.inside(deleted))
            for mask in self.blockers(deleted).values():result&=any_edges(mask)
            self.stable_cache[deleted]=result
        return self.stable_cache[deleted]

    def good(self,deleted,remaining):
        key=deleted,remaining
        if key in self.goods:return self.goods[key]
        if remaining==0:return self.leaf_bad(deleted)
        result=ALL
        for f,team in enumerate(self.teams(deleted)):
            if len(team)!=2:continue
            bit=edge(*team)
            children=ALL
            for w in team:children&=self.good(deleted|(1<<(f*N+w-1)),remaining-1)
            result&=(ALL^any_edges(bit))|children
            if not result:break
        result&=any_edges(self.inside(deleted))|self.leaf_bad(deleted)
        self.goods[key]=result
        return result

    def best_target(self,root_good):
        best=None
        for f,team in enumerate(self.teams(0)):
            if len(team)!=2:continue
            for r,s in (team,team[::-1]):
                d1=1<<(f*N+r-1)
                for g,second in enumerate(self.teams(d1)):
                    if len(second)!=2:continue
                    for t,u in (second,second[::-1]):
                        d2=1<<(g*N+t-1);both=d1|d2
                        old=f*N+r-1;latest=g*N+t-1
                        blockers=self.blockers(both)
                        if old not in blockers:continue
                        graphs=root_good&any_edges(edge(r,s))&any_edges(edge(t,u))
                        graphs&=ALL^any_edges(self.inside(both))
                        graphs&=ALL^any_edges(blockers[old])
                        if latest in blockers:graphs&=any_edges(blockers[latest])
                        if old in self.blockers(d1):graphs&=any_edges(self.blockers(d1)[old])
                        if not graphs:continue
                        restored=self.state(d2)
                        obstructed=restored[r-1]!=f+1 or restored[s-1]!=f+1
                        if not obstructed:continue
                        exact=restored[r-1]==f+1 and restored[s-1]!=f+1
                        exact_graphs=graphs&self.leaf_stable(d2) if exact else 0
                        if exact_graphs:graphs=exact_graphs
                        else:exact=False
                        full=all(len(x)==2 for x in self.teams(both))
                        # Smallest integer graph is an initial witness; a later
                        # pass explicitly minimizes its number of forbidden edges.
                        graph=(graphs&-graphs).bit_length()-1
                        score=(exact,full,r==t,f!=g,-graph.bit_count())
                        if best is None or score>best['score']:
                            best=dict(score=score,graph=graph,first=[f+1,r,s],second=[g+1,t,u],
                                      exact_restoration=exact,full_target=full,
                                      same_removed_worker=r==t,qualifying_graphs=graphs.bit_count())
        return best


def direct_good(p,graph,max_depth=2):
    m=Model(graph_problem(p,graph))
    @lru_cache(None)
    def visit(deleted):
        z=m.full_da(deleted)
        if not z['conflicts']:
            return bool(blocker_pairs(m,z['assignment'],set()))
        if len(deleted)>=max_depth:return False
        return all(visit(deleted|{(pair['firm'],w)}) for pair in z['conflicts']
                   for w in (pair['left'],pair['right']))
    return visit(frozenset())


def neighbors(p):
    rows=p['worker_rankings_one_based']+p['firm_rankings_one_based']
    for index,row in enumerate(rows):
        choices=[]
        for i in range(len(row)-1):
            changed=row.copy();changed[i],changed[i+1]=changed[i+1],changed[i]
            choices.append((changed,f'row{index}:swap{i}'))
        if row:choices.append((row[:-1],f'row{index}:remove_last'))
        size=F if index<N else N
        for label in range(1,size+1):
            if label not in row:choices.append((row+[label],f'row{index}:append{label}'))
        for changed,description in choices:
            altered=[x.copy() for x in rows];altered[index]=changed
            yield rankings_problem(altered[:N],altered[N:]),description


def run(args):
    original=json.loads(args.seed.read_text())
    seed=rankings_problem(original['worker_rankings_one_based'],original['firm_rankings_one_based'])
    rng=random.Random(20261104)
    kernel=Symbolic(seed);initial=kernel.good(0,2)
    test_graphs=[0,(1<<21)-1]+[rng.randrange(1<<21) for _ in range(args.self_checks)]
    for graph in test_graphs:assert bool(initial&(1<<graph))==direct_good(seed,graph),graph
    print('symbolic/direct self-checks passed',len(test_graphs),flush=True)
    queue=deque([(seed,[])])
    seen={key_of(seed)}
    checked=positive=0;best=None;started=time.monotonic()
    args.output.mkdir(parents=True,exist_ok=True)
    log=(args.output/'profile_scan.jsonl').open('w')
    while queue and checked<args.max_profiles:
        p,history=queue.popleft()
        symbolic=Symbolic(p);good=symbolic.good(0,2);checked+=1
        target=symbolic.best_target(good) if good else None
        record=dict(index=checked,edit_depth=len(history),edits=history,
                    preference_sha256=hashlib.sha256(repr(key_of(p)).encode()).hexdigest(),
                    all_fail_depth_two_graphs=good.bit_count(),interference_target=target)
        log.write(json.dumps(record)+'\n')
        if good:
            positive+=1
            graph=target['graph'] if target else (good&-good).bit_length()-1
            witness=dict(problem=graph_problem(p,graph),graph=graph,edits=history,target=target,
                         all_fail_depth_two_graphs=good.bit_count())
            if (best is None or (target is not None and (best['target'] is None or
                    target['score']>best['target']['score']))):
                best=witness;(args.output/'best_witness.json').write_text(json.dumps(witness,indent=2)+'\n')
            if target and target['exact_restoration']:
                assert direct_good(p,graph)
                for g in [graph]+[rng.randrange(1<<21) for _ in range(32)]:
                    assert bool(good&(1<<g))==direct_good(p,g)
                print('FOUND',json.dumps(record),flush=True)
                break
        if len(history)<args.radius:
            for q,description in neighbors(p):
                key=key_of(q)
                if key not in seen:seen.add(key);queue.append((q,history+[description]))
        if checked%args.progress_every==0:
            log.flush();print(f'profiles={checked} positive={positive} queued={len(queue)} elapsed={time.monotonic()-started:.1f}s',flush=True)
    log.close()
    summary=dict(preference_space_exhaustive=False,forbidden_graphs_per_profile=COUNT,
                 seed=str(args.seed.resolve()),radius=args.radius,max_profiles=args.max_profiles,
                 profiles_checked=checked,profiles_with_depth_two_all_fail=positive,
                 remaining_queued_profiles=len(queue),discovered_profiles=len(seen),
                 neighborhood_exhausted=not queue,
                 exact_interference_example_found=bool(best and best['target'] and best['target']['exact_restoration']),
                 elapsed_seconds=time.monotonic()-started)
    (args.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--seed',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--radius',type=int,default=2)
    parser.add_argument('--max-profiles',type=int,default=10000)
    parser.add_argument('--self-checks',type=int,default=64)
    parser.add_argument('--progress-every',type=int,default=250)
    run(parser.parse_args())
