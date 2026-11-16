"""Direct SAT/SMT search over preferences for an explicitly restricted tree shape.

Root: f1={w1,w2}, f2={w3,w4}, f3={w5,w6}, w7 unmatched;
only {w1,w2} conflicts at root, and each child has at most one conflict.
Both complete and incomplete strict preferences are allowed. UNSAT here would
cover this shape only, not the entire 3x7 preference/input space.
"""
import argparse
import itertools
import json
import sys
import time
from pathlib import Path

sys.path.insert(0,'/private/tmp/ancestor-z3')
import z3

N,F=7,3


class Encoder:
    def __init__(self,timeout,rounds=21):
        self.s=z3.SolverFor('QF_FD')
        self.s.set(timeout=timeout)
        self.worker=[self.order(f'w{w}',F+1) for w in range(N)]
        self.firm=[self.order(f'f{f}',N+1) for f in range(F)]
        self.cp={(a,b):z3.Bool(f'cp_{a}_{b}') for a,b in itertools.combinations(range(N),2)}
        self.instances={}
        self.rounds=rounds

    def order(self,name,size):
        variables={(a,b):z3.Bool(f'{name}_{a}_above_{b}') for a,b in itertools.combinations(range(size),2)}
        for a,b,c in itertools.combinations(range(size),3):
            ab,bc,ac=variables[a,b],variables[b,c],variables[a,c]
            self.s.add(z3.Or(z3.Not(ab),z3.Not(bc),ac),z3.Or(ab,bc,z3.Not(ac)))
        return variables

    @staticmethod
    def better(order,a,b):
        if a==b:return z3.BoolVal(False)
        return order[a,b] if a<b else z3.Not(order[b,a])

    def forbidden(self,a,b):return self.cp[min(a,b),max(a,b)]

    def da(self,name,deleted):
        s=self.s
        match=[[z3.BoolVal(False) for _ in range(F)] for _ in range(N)]
        proposed=[[z3.BoolVal(False) for _ in range(F)] for _ in range(N)]
        for step in range(self.rounds):
            available=[[z3.And(z3.Not(z3.Or(match[w])),z3.Not(proposed[w][f]),
                               z3.Not(deleted(w,f)),self.better(self.worker[w],f,F),
                               self.better(self.firm[f],w,N)) for f in range(F)] for w in range(N)]
            offer=[[z3.Bool(f'{name}_q_{step}_{w}_{f}') for f in range(F)] for w in range(N)]
            for w in range(N):
                for f in range(F):
                    s.add(offer[w][f]==z3.And(available[w][f],*[z3.Implies(available[w][g],self.better(self.worker[w],f,g)) for g in range(F) if g!=f]))
            candidate=[[z3.Or(match[w][f],offer[w][f]) for f in range(F)] for w in range(N)]
            following=[[z3.Bool(f'{name}_m_{step}_{w}_{f}') for f in range(F)] for w in range(N)]
            for w in range(N):
                for f in range(F):
                    superior=[z3.And(candidate[v][f],self.better(self.firm[f],v,w)) for v in range(N) if v!=w]
                    s.add(following[w][f]==z3.And(candidate[w][f],z3.AtMost(*superior,1)))
            proposed=[[z3.Or(proposed[w][f],offer[w][f]) for f in range(F)] for w in range(N)]
            match=following
        # Require an actual completed DA even when using a shorter circuit.
        for w in range(N):
            for f in range(F):
                s.add(z3.Implies(z3.Not(z3.Or(match[w])),z3.Or(proposed[w][f],deleted(w,f),
                    z3.Not(self.better(self.worker[w],f,F)),z3.Not(self.better(self.firm[f],w,N)))))
        self.instances[name]=match
        return match

    def feasible(self,match):
        return z3.And(*[z3.Not(z3.And(match[a][f],match[b][f],self.forbidden(a,b)))
                        for f in range(F) for a,b in itertools.combinations(range(N),2)])

    def ur(self,match,w,f):
        prefers=z3.Or(z3.And(z3.Not(z3.Or(match[w])),self.better(self.worker[w],f,F)),
                      *[z3.And(match[w][g],self.better(self.worker[w],f,g)) for g in range(F) if g!=f])
        improves=z3.Or(z3.And(z3.Not(z3.AtLeast(*[match[v][f] for v in range(N)],2)),self.better(self.firm[f],w,N)),
                       *[z3.And(match[v][f],self.better(self.firm[f],w,v)) for v in range(N) if v!=w])
        compatible=z3.And(*[z3.Implies(match[v][f],z3.Not(self.forbidden(w,v))) for v in range(N) if v!=w])
        return z3.And(prefers,improves,compatible)

    def stable(self,match):
        return z3.And(self.feasible(match),*[z3.Not(self.ur(match,w,f)) for w in range(N) for f in range(F)])

    def pin(self,p):
        for w,row in enumerate(p['worker_rankings_one_based']):
            ranking=[f-1 for f in row]+[F]+[f for f in range(F) if f+1 not in row]
            for (a,b),variable in self.worker[w].items():self.s.add(variable==(ranking.index(a)<ranking.index(b)))
        for f,row in enumerate(p['firm_rankings_one_based']):
            ranking=[w-1 for w in row]+[N]+[w for w in range(N) if w+1 not in row]
            for (a,b),variable in self.firm[f].items():self.s.add(variable==(ranking.index(a)<ranking.index(b)))

    def decode(self,model):
        def rows(orders,size):
            result=[]
            for order in orders:
                wins={a:sum(z3.is_true(model.eval(self.better(order,a,b),model_completion=True)) for b in range(size) if a!=b) for a in range(size)}
                ranking=sorted(range(size),key=lambda a:-wins[a]);cut=ranking.index(size-1)
                result.append([a+1 for a in ranking[:cut]])
            return result
        return rows(self.worker,F+1),rows(self.firm,N+1)


def control(args):
    from search_symbolic import Model
    p=json.loads(args.seed.read_text());m=Model(p);encoder=Encoder(args.timeout)
    encoder.pin(p)
    states={};differences=[]
    for label,deleted in [('root',set()),('one',{(1,6)}),('two',{(1,6),(3,5)})]:
        state=encoder.da(label,lambda w,f,d=deleted:z3.BoolVal((f+1,w+1) in d));states[label]=state
        expected=m.full_da(frozenset(deleted))['assignment']
        differences.extend(state[w][f]!=(expected[w]==f+1) for w in range(N) for f in range(F))
    result=encoder.s.check()
    assert result==z3.sat,result
    encoder.s.add(z3.Or(*differences))
    differential=encoder.s.check()
    assert differential==z3.unsat,differential
    return dict(fixed_profile_da_control='sat',different_assignment_control='unsat',
                instances=len(states),z3_version=z3.get_version_string())


def search(args):
    encoder=Encoder(args.timeout,args.rounds);s=encoder.s
    root=encoder.da('root',lambda w,f:z3.BoolVal(False))
    for w in range(N):
        for f in range(F):s.add(root[w][f]==(w<6 and w//2==f))
    s.add(encoder.forbidden(0,1),z3.Not(encoder.forbidden(2,3)),z3.Not(encoder.forbidden(4,5)))
    branches=[]
    for removed in (0,1):
        child=encoder.da(f'child{removed}',lambda w,f,r=removed:z3.BoolVal(w==r and f==0))
        conflicts={(f,a,b):z3.And(child[a][f],child[b][f],encoder.forbidden(a,b))
                   for f in range(F) for a,b in itertools.combinations(range(N),2)}
        s.add(z3.AtMost(*conflicts.values(),1));active=z3.Or(*conflicts.values())
        def deletion(w,f,upper=False,c=conflicts):
            return z3.Or(*[flag for (h,a,b),flag in c.items() if h==f and (b if upper else a)==w])
        low=lambda w,f,d=deletion:d(w,f,False)
        high=lambda w,f,d=deletion:d(w,f,True)
        left=encoder.da(f'leaf{removed}low',lambda w,f,r=removed,d=low:z3.Or(w==r and f==0,d(w,f)))
        right=encoder.da(f'leaf{removed}high',lambda w,f,r=removed,d=high:z3.Or(w==r and f==0,d(w,f)))
        s.add(z3.Implies(z3.Not(active),encoder.ur(child,removed,0)))
        for leaf,deleted in [(left,low),(right,high)]:
            latest=z3.Or(*[z3.And(deleted(w,f),encoder.ur(leaf,w,f)) for w in range(N) for f in range(F)])
            s.add(z3.Implies(active,z3.And(encoder.feasible(leaf),z3.Or(encoder.ur(leaf,removed,0),latest))))
        branches.append((child,active,left,right,low,high))
    child,active,left,right,low,high=branches[1]
    choose_left=z3.Bool('target_left')
    target=[[z3.If(choose_left,left[w][f],right[w][f]) for f in range(F)] for w in range(N)]
    latest=lambda w,f:z3.If(choose_left,low(w,f),high(w,f))
    restored=encoder.da('restore_first',latest)
    s.add(active,encoder.ur(target,1,0),z3.Not(encoder.ur(child,1,0)))
    s.add(*[z3.Implies(latest(w,f),z3.Not(encoder.ur(target,w,f))) for w in range(N) for f in range(F)])
    if args.counter_mode=='exact':
        s.add(restored[1][0],z3.Not(restored[0][0]),encoder.stable(restored))
    elif args.counter_mode=='obstructed':
        s.add(z3.Not(z3.And(restored[1][0],restored[0][0])))
    if args.full_target:s.add(*[z3.AtLeast(*[target[w][f] for w in range(N)],2) for f in range(F)])
    # These symmetry breaks preserve at least one representative of this shape.
    s.add(encoder.better(encoder.firm[1],2,3),encoder.better(encoder.firm[2],4,5))
    started=time.monotonic();print('SMT encoding built; finite-domain constraints compiled to SAT clauses',flush=True)
    result=s.check();report=dict(status=str(result),scope='root single conflict, at most one conflict in each child, full root',
        whole_preference_space_exhaustive=False,timeout_ms=args.timeout,full_target=args.full_target,
        da_rounds_bound=args.rounds,counter_mode=args.counter_mode,
        z3_version=z3.get_version_string(),elapsed_seconds=time.monotonic()-started,
        statistics=str(s.statistics()))
    if result==z3.sat:
        model=s.model();workers,firms=encoder.decode(model)
        report['worker_rankings_one_based']=workers;report['firm_rankings_one_based']=firms
        report['forbidden_pairs_one_based']=[[a+1,b+1] for (a,b),flag in encoder.cp.items() if z3.is_true(model.eval(flag,model_completion=True))]
    elif result==z3.unknown:report['reason_unknown']=s.reason_unknown()
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--seed',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--timeout',type=int,default=180000)
    parser.add_argument('--control-only',action='store_true')
    parser.add_argument('--full-target',action='store_true')
    parser.add_argument('--rounds',type=int,default=21)
    parser.add_argument('--counter-mode',choices=['exact','obstructed','new'],default='exact')
    args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True)
    report=control(args) if args.control_only else search(args)
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
