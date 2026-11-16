"""Build reproducible examples and check them against unmodified repository C++."""
import itertools
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent))
from render_da_search_trees import Model, blocker_pairs, render, verify
from analyze_candidates import problem_from_witness


def rankings_problem(workers, firms, forbidden):
    def scores(orders, n):
        return [[len(row) - row.index(x + 1) if x + 1 in row else -100000
                 for x in range(n)] for row in orders]
    cp = [[0] * len(workers) for _ in workers]
    for a, b in forbidden:
        cp[a - 1][b - 1] = cp[b - 1][a - 1] = -100000
    return dict(n_workers=len(workers), n_firms=len(firms), capacities=[2, 2],
                agent_prefs=scores(workers, len(firms)), firm_prefs=scores(firms, len(workers)),
                colleague_prefs=cp, worker_rankings_one_based=workers,
                firm_rankings_one_based=firms, forbidden_pairs_one_based=forbidden)


def relabel_witness(x):
    # First firm becomes f1; first removed/retained workers become w1/w2;
    # second removed/retained workers become w3/w4, remaining worker w5.
    old = problem_from_witness(x)
    f, r, s = x['first']
    g, t, u = x['second']
    worker_order = [r, s, t, u]
    if len(set(worker_order)) != 4:
        return old, tuple(x['first'][:2]), tuple(x['second'][:2])
    worker_order += [w for w in range(1, 6) if w not in worker_order]
    worker_map = {w: i + 1 for i, w in enumerate(worker_order)}
    firm_order = [f, g]
    firm_map = {h: i + 1 for i, h in enumerate(firm_order)}
    workers = [[firm_map[h] for h in old['worker_rankings_one_based'][w - 1]]
               for w in worker_order]
    firms = [[worker_map[w] for w in old['firm_rankings_one_based'][h - 1]]
             for h in firm_order]
    forbidden = [sorted([worker_map[a], worker_map[b]])
                 for a, b in old['forbidden_pairs_one_based']]
    return rankings_problem(workers, firms, sorted(forbidden)), (1, 1), (2, 3)


def assignment_text(a):
    groups = [[f'w{w}' for w, h in enumerate(a, 1) if h == f] for f in (1, 2, 0)]
    return ['{' + ','.join(group) + '}' if group else 'なし' for group in groups]


def build_case(name, p, d1, d2, bridge):
    dest = ROOT / 'results' / name
    dest.mkdir(parents=True, exist_ok=True)
    (dest / 'problem.json').write_text(json.dumps(p, ensure_ascii=False, indent=2) + '\n')
    text = f'{p["n_workers"]} {p["n_firms"]}\n'
    text += ' '.join(map(str, p['capacities'])) + '\n'
    for key in ('agent_prefs', 'firm_prefs', 'colleague_prefs'):
        text += ''.join(' '.join(map(str, row)) + '\n' for row in p[key])
    (dest / 'problem.txt').write_text(text)
    subprocess.run([bridge, str(dest / 'recursive_doctor.json')], input=text, text=True, check=True)
    tree = json.loads((dest / 'recursive_doctor.json').read_text())
    m = Model(p)
    verification = verify(tree, m)
    (dest / 'recursive_doctor.md').write_text(render(tree, m) + '\n')
    evidence = []
    for label, deleted in [('P0_original', set()), ('P1_first_only', {d1}),
                           ('P2_both', {d1, d2}), ('P3_second_only', {d2})]:
        state = m.full_da(frozenset(deleted))
        state['label'] = label
        state['original_blocking_pairs'] = blocker_pairs(m, state['assignment'], set())
        state['reduced_blocking_pairs'] = blocker_pairs(m, state['assignment'], deleted)
        state['original_ur_stable'] = not state['conflicts'] and not state['original_blocking_pairs']
        evidence.append(state)
    a, b, c, restored = evidence
    assert not blocker_pairs(m, b['assignment'], set())
    assert c['original_blocking_pairs'] == [{'firm': d1[0], 'worker': d1[1]}]
    assert not c['conflicts'] and not c['reduced_blocking_pairs']
    assert restored['original_ur_stable']
    assert restored['assignment'][d1[1] - 1] == d1[0]
    assert max(node['da_run'] for node in tree['nodes']) == 3

    # Exhaustively list feasible original-market UR-stable assignments as a
    # separate audit. 0 denotes unmatched, not a third firm.
    stable = []
    for assignment in itertools.product(range(3), repeat=p['n_workers']):
        if any(sum(h == f for h in assignment) > p['capacities'][f - 1] for f in (1, 2)):
            continue
        if m.conflicts(assignment):
            continue
        if any(h and (p['agent_prefs'][w][h - 1] <= 0 or p['firm_prefs'][h - 1][w] <= 0)
               for w, h in enumerate(assignment)):
            continue
        if not blocker_pairs(m, assignment, set()):
            stable.append(assignment)
    verification['all_243_assignments_audited'] = True
    verification['original_ur_stable_assignments'] = stable
    verification['restored_first_deletion_original_ur_stable'] = True
    verification['target_deletions'] = [d1, d2]
    verification['maximum_deletion_depth'] = 2
    (dest / 'verification.json').write_text(json.dumps(verification, indent=2) + '\n')
    (dest / 'counterfactual_traces.json').write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n')
    lines = ['# 削除の比較と提案履歴', '',
             '| 問題 | 削除 | f1 | f2 | 未マッチ | 元問題でのUR安定 |',
             '|---|---|---|---|---|---|']
    for z in evidence:
        groups = assignment_text(z['assignment'])
        deleted = ', '.join(f'(f{h},w{w})' for h, w in z['deleted_before']) or 'なし'
        lines.append('| ' + ' | '.join([z['label'], deleted] + groups +
                     ['はい' if z['original_ur_stable'] else 'いいえ']) + ' |')
    for z in evidence:
        lines += ['', '## ' + z['label'], '',
                  '| 提案番号 | 個人 | 企業 | 結果 | f1 | f2 | 未マッチ |',
                  '|---|---|---|---|---|---|---|']
        for i, proposal in enumerate(z['proposals'], 1):
            lines.append('| ' + ' | '.join([str(i), f'w{proposal["worker"]}',
                         f'f{proposal["firm"]}', proposal['reason']] +
                         assignment_text(proposal['assignment_after'])) + ' |')
    (dest / 'counterfactual_traces.md').write_text('\n'.join(lines) + '\n')
    index = [f'# {name}', '', '[例の説明・全探索の範囲・再現方法](../../README.md)', '',
             f'対象経路の削除は (f{d1[0]},w{d1[1]}) → (f{d2[0]},w{d2[1]})。', '',
             '- [入力選好](problem.json)', '- [全分岐図](recursive_doctor.md)',
             '- [削除を戻す比較と全提案履歴](counterfactual_traces.md)',
             '- [検算結果と元問題の全UR安定割当](verification.json)', '']
    (dest / 'README.md').write_text('\n'.join(index))
    print(name, json.dumps(verification, ensure_ascii=False))


def main():
    bridge = sys.argv[1]
    # Two disjoint forbidden pairs; all firms/workers mutually acceptable.
    # Four workers share f2 > f1; only w5 has f1 > f2.
    p = rankings_problem([[2, 1]] * 4 + [[1, 2]],
                         [[4, 2, 1, 5, 3], [4, 5, 3, 2, 1]], [[1, 2], [3, 4]])
    build_case('main_two_forbidden_pairs', p, (1, 1), (2, 3), bridge)
    x = json.loads((ROOT / 'candidate_analysis.json').read_text())['complete']['best'][0]['witness']
    p, d1, d2 = relabel_witness(x)
    build_case('compact_three_leaves', p, d1, d2, bridge)


if __name__ == '__main__':
    main()
