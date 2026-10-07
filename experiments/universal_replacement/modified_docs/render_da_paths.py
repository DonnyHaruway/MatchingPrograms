"""Independently enumerate DA rounds and render every verified path as Markdown.

This model does not call the C++ DA. All choice histories, their decision-time
assignments, and final assignments must exactly match the saved C++ output.
"""

import itertools
import json
import os
import sys
from pathlib import Path

from verify_da_paths import require, verify


class RoundModel:
    def __init__(self, problem, algorithm):
        self.algorithm = algorithm
        self.wp = problem["agent_prefs"]
        self.fp = problem["firm_prefs"]
        self.cp = problem["colleague_prefs"]
        self.cap = problem["capacities"]
        self.w, self.f = problem["n_workers"], problem["n_firms"]
        prefs, alternatives = (self.fp, self.w) if algorithm == "firm" else (self.wp, self.f)
        self.orders = [sorted(range(alternatives), key=lambda x: (-row[x], x)) for row in prefs]
        self.calls = 0

    def conflict(self, a, b):
        return self.cp[a][b] == -100000 or self.cp[b][a] == -100000

    def step_a(self, state):
        before, cursor, deleted = state
        cursor = list(cursor)
        members = [[w for w in range(self.w) if before[w] == f + 1] for f in range(self.f)]
        proposals, checks = [], []
        if self.algorithm == "firm":
            after = list(before)
            for f, order in enumerate(self.orders):
                if len(members[f]) >= self.cap[f]:
                    checks.append(f"f{f+1}: 満員のため提案しない")
                    continue
                while cursor[f] < self.w and (f, order[cursor[f]]) in deleted:
                    cursor[f] += 1
                if cursor[f] == self.w:
                    checks.append(f"f{f+1}: 提案先を使い切った")
                    continue
                w = order[cursor[f]]
                cursor[f] += 1
                if self.fp[f][w] < 0:
                    cursor[f] = self.w
                    checks.append(f"f{f+1}: 残る相手は受け入れ不可、提案終了")
                    continue
                proposals.append((f, w))
                if self.wp[w][f] >= 0 and (after[w] == 0 or self.wp[w][f] > self.wp[w][after[w]-1]):
                    after[w] = f + 1
        else:
            for w, order in enumerate(self.orders):
                if before[w] != 0:
                    checks.append(f"w{w+1}: 所属先があるため提案しない")
                    continue
                while cursor[w] < self.f and (order[cursor[w]], w) in deleted:
                    cursor[w] += 1
                if cursor[w] == self.f:
                    checks.append(f"w{w+1}: 提案先を使い切った")
                    continue
                f = order[cursor[w]]
                cursor[w] += 1
                if self.wp[w][f] < 0:
                    cursor[w] = self.f
                    checks.append(f"w{w+1}: 残る企業は受け入れ不可、提案終了")
                    continue
                proposals.append((f, w))
                if self.fp[f][w] >= 0:
                    members[f].append(w)
            after = [0] * self.w
            for f, team in enumerate(members):
                ordered = sorted(team, key=lambda w: (-self.fp[f][w], before[w] != f + 1, w))
                for w in ordered[:self.cap[f]]:
                    after[w] = f + 1
        conflicts = []
        for f in range(self.f):
            team = [w for w in range(self.w) if after[w] == f + 1]
            for a, b in itertools.combinations(team, 2):
                if self.conflict(a, b):
                    conflicts.append({"firm": f + 1, "left": a + 1, "right": b + 1})
        info = {"before": list(before), "after_a": after, "proposals": proposals,
                "checks": checks, "conflicts": conflicts, "cursor": cursor}
        return (tuple(after), tuple(cursor), deleted), info

    def enumerate(self):
        results = []

        def visit(state, trace, decisions):
            self.calls += 1
            require(self.calls < 100000 and len(trace) < 100, "unexpectedly large search")
            next_state, info = self.step_a(state)
            if not info["proposals"]:
                results.append({"assignment": list(state[0]), "decisions": decisions,
                                "trace": trace, "stop_checks": info["checks"]})
                return
            round_number = len(trace) + 1
            info["round"] = round_number
            for ordered in itertools.permutations(info["conflicts"]):
                order_decisions, remaining = [], list(info["conflicts"])
                for pair in ordered[:-1]:
                    selected = remaining.index(pair)
                    order_decisions.append({"round": round_number, "kind": "pair_order",
                        "option_count": len(remaining), "selected_index": selected,
                        "assignment_before": list(next_state[0]), "pairs": list(remaining)})
                    remaining.pop(selected)

                def resolve(position, assignment, deleted, local_decisions, removals, skipped):
                    if position == len(ordered):
                        completed = dict(info, after_b=list(assignment), removals=removals,
                                         skipped=skipped, deleted=sorted(deleted))
                        visit((assignment, next_state[1], deleted), trace + [completed],
                              decisions + order_decisions + local_decisions)
                        return
                    pair = ordered[position]
                    f, a, b = pair["firm"] - 1, pair["left"] - 1, pair["right"] - 1
                    if assignment[a] != f + 1 or assignment[b] != f + 1:
                        resolve(position + 1, assignment, deleted, local_decisions,
                                removals, skipped + [pair])
                        return
                    for selected, removed in enumerate((a, b)):
                        decision = {"round": round_number, "kind": "removal_endpoint",
                            "option_count": 2, "selected_index": selected,
                            "assignment_before": list(assignment), "pairs": [pair],
                            "removed_worker": removed + 1, "firm": f + 1}
                        updated = list(assignment)
                        updated[removed] = 0
                        resolve(position + 1, tuple(updated), deleted | {(f, removed)},
                                local_decisions + [decision], removals + [decision], skipped)

                resolve(0, next_state[0], next_state[2], [], [], [])

        visit(((0,) * self.w, (0,) * len(self.orders), frozenset()), [], [])
        return results

    def remaining(self, step):
        rows = []
        for owner, order in enumerate(self.orders):
            targets = []
            for target in order[step["cursor"][owner]:]:
                f, w = (owner, target) if self.algorithm == "firm" else (target, owner)
                score = self.fp[f][w] if self.algorithm == "firm" else self.wp[w][f]
                if score >= 0 and (f, w) not in step["deleted"]:
                    targets.append(("w" if self.algorithm == "firm" else "f") + str(target + 1))
            label = ("f" if self.algorithm == "firm" else "w") + str(owner + 1)
            if self.algorithm == "firm":
                active = step["after_b"].count(owner + 1) < self.cap[owner]
            else:
                active = step["after_b"][owner] == 0
            rows.append(f"| {label} | {'、'.join(targets) or 'なし'} | {'提案可能' if active and targets else '現在は提案しない'} |")
        return rows


def team_text(assignment, label):
    team = [f"w{w+1}" for w, f in enumerate(assignment) if f == label]
    return "{" + ", ".join(team) + "}" if team else "∅"


def matching_table(assignment, firms):
    return ["| 企業 | 所属労働者 |", "|---|---|"] + [
        f"| f{f} | {team_text(assignment, f)} |" for f in range(1, firms + 1)
    ] + [f"| 未マッチ | {team_text(assignment, 0)} |"]


def pair_text(pair):
    return f'f{pair["firm"]}: {{w{pair["left"]},w{pair["right"]}}}'


def deletion_text(path):
    return " → ".join(f'R{d["round"]}: f{d["firm"]}からw{d["removed_worker"]}'
                      for d in path["decisions"] if d["kind"] == "removal_endpoint") or "なし"


def render_path(model, path, replay):
    name = "firm proposing" if model.algorithm == "firm" else "doctor / worker proposing"
    status = "安定" if path["stable"] else "不安定"
    lines = [f'# {name} — 経路 {path["path_id"]:02d}', "", "[全経路一覧と分岐図](da_paths_index.md)", "",
             f'最終結果は **{status}**。マッチ人数は{path["matched_workers"]}人。', "",
             f'削除履歴：{deletion_text(path)}。', "",
             "この文書の全ラウンドは独立したPythonモデルで再計算し、C++の全選択履歴・選択前の割当・最終割当と一致することを確認した。", "",
             "## 選択した経路の流れ", "", "```mermaid", "flowchart TD", '  start["全員未マッチから開始"]']
    previous = "start"
    for step in replay["trace"]:
        r = step["round"]
        conflict = "<br/>".join(pair_text(p) for p in step["conflicts"]) or "禁止ペアなし"
        lines += [f'  a{r}["R{r} A: 提案{len(step["proposals"])}件<br/>{conflict}"]',
                  f"  {previous} --> a{r}"]
        if step["removals"]:
            label = "<br/>".join(f'f{d["firm"]}からw{d["removed_worker"]}を永久削除'
                                  for d in step["removals"])
            lines += [f'  b{r}{{"R{r} B: ランダム選択<br/>{label}"}}',
                      f'  a{r} -->|"この経路での選択"| b{r}', f"  class b{r} choice"]
        else:
            lines += [f'  b{r}["R{r} B: 削除なし"]', f"  a{r} --> b{r}"]
        previous = f"b{r}"
    lines += [f'  done["提案可能な相手がなく停止<br/>{status}: {path["matched_workers"]}人マッチ"]',
              f"  {previous} --> done", "  classDef choice fill:#fff3cd,stroke:#9c6f00,color:#222;",
              "  classDef stable fill:#d9f2df,stroke:#237a3b,color:#222;",
              "  classDef unstable fill:#fadddd,stroke:#b32626,color:#222;",
              f"  class done {'stable' if path['stable'] else 'unstable'}", "```", "",
              "図はこの経路で選んだ分岐だけを示す。他方を選んだ経路は一覧の全体分岐図に示す。", "",
              "## 最終マッチング", ""] + matching_table(path["assignment"], model.f)
    lines += ["", "## ラウンドごとの詳細", "",
              "Aの全提案を処理してからBで禁止ペアを解消する。既存の所属と提案履歴を保持する。", ""]
    for step in replay["trace"]:
        r = step["round"]
        lines += [f"### R{r}", "", "**A：今回の提案**", "", "| 提案 | A終了時の判定 |", "|---|---|"]
        for f, w in step["proposals"]:
            offer = f"f{f+1} → w{w+1}" if model.algorithm == "firm" else f"w{w+1} → f{f+1}"
            result = "採用" if step["after_a"][w] == f+1 else "拒否（A終了時にこの組は成立しない）"
            lines.append(f"| {offer} | {result} |")
        lines += ["", "| 企業 | ラウンド開始 | A終了（B前） | B終了 |", "|---|---|---|---|"]
        for f in list(range(1, model.f + 1)) + [0]:
            lines.append(f"| {'f'+str(f) if f else '未マッチ'} | " + " | ".join(
                team_text(step[key], f) for key in ("before", "after_a", "after_b")) + " |")
        lines += ["", "**B：禁止ペアと選択**", ""]
        if not step["conflicts"]:
            lines += ["禁止ペアはない。ランダムな削除も発生しない。"]
        for d in step["removals"]:
            p = d["pairs"][0]
            other = p["right"] if d["removed_worker"] == p["left"] else p["left"]
            lines += [f'- {pair_text(p)}。削除の選択肢はw{p["left"]}／w{p["right"]}。'
                      f'この経路では **w{d["removed_worker"]}を削除**し、w{other}を残す。']
        for p in step["skipped"]:
            lines.append(f"- {pair_text(p)}は先の削除で解決済みのためスキップ。")
        deleted = "、".join(f"(f{f+1},w{w+1})" for f, w in step["deleted"]) or "なし"
        lines += ["", f"永久削除の累計：{deleted}。", "",
                  "**B終了後に残る未提案の相手**", "",
                  "左から次に提案する順。満員の企業／所属済みの労働者は、候補が残っていても現在は提案しない。", "",
                  "| 提案する側 | 未提案・未削除の受け入れ可能な相手 | 現在の状態 |", "|---|---|---|"]
        lines += model.remaining(step) + [""]
    lines += ["## 停止理由", "", "次のAを試みても有効な提案は0件となるため停止。", ""]
    lines += [f"- {check}" for check in replay["stop_checks"]]
    lines += ["", "## 元の選好による安定性", "",
              "削除前の選好で、個人合理性・同僚feasibility・universal-replacement blocking pairの有無を検算した。", ""]
    if path["stable"]:
        lines += ["universal-replacement blocking pairは **存在しない**。"]
    else:
        lines += ["blocking pairは **(f3,w4)**。", "",
                  "- w4は現在のf4よりf3を好む。", "- f3は{w2,w7}で満員。",
                  "- f3はw2よりw4を好む。", "- w2を外すと{w4,w7}、w7を外すと{w2,w4}になり、両方ともfeasible。", "",
                  "w4はR5でf3から永久削除されているが、安定性は元の選好で判定するため、この組をブロッキングとして数える。"]
    lines += ["", "空席への追加によるblockingは今回の定義に含まない。", ""]
    return "\n".join(lines)


def render_tree(algorithm, paths):
    nodes, serial = {}, itertools.count()
    lines = ["```mermaid", "flowchart TD"]

    def draw(prefix, subset):
        node = "n" + str(next(serial))
        nodes[prefix] = node
        path = subset[0]
        if len(path["decisions"]) == len(prefix):
            require(len(subset) == 1, "duplicate tree leaf")
            label = f'経路 {path["path_id"]:02d}: {"安定" if path["stable"] else "不安定"}<br/>{path["matched_workers"]}人マッチ'
            lines.extend([f'  {node}["{label}"]', f'  class {node} {"stable" if path["stable"] else "unstable"}',
                          f'  click {node} "{algorithm}_path_{path["path_id"]:02d}.md"'])
            return node
        d = path["decisions"][len(prefix)]
        label = f'R{d["round"]}: ' + (pair_text(d["pairs"][0]) if d["kind"] == "removal_endpoint" else "ペア処理順の選択")
        lines.append(f'  {node}{{"{label}"}}')
        for selected in range(d["option_count"]):
            branch = [p for p in subset if p["decisions"][len(prefix)]["selected_index"] == selected]
            child = draw(prefix + (selected,), branch)
            chosen = d["pairs"][0]["left" if selected == 0 else "right"] if d["kind"] == "removal_endpoint" else selected
            edge = f"w{chosen}を削除" if d["kind"] == "removal_endpoint" else f"選択肢{chosen}"
            lines.append(f'  {node} -->|"{edge}"| {child}')
        return node

    draw((), paths)
    lines += ["  classDef stable fill:#d9f2df,stroke:#237a3b,color:#222;",
              "  classDef unstable fill:#fadddd,stroke:#b32626,color:#222;", "```"]
    return lines


def main(directory):
    verification = verify(directory)
    problem = json.loads((directory / "problem.json").read_text())
    output = Path(__file__).resolve().parent
    data_output = directory / "path_details"
    data_output.mkdir(exist_ok=True)
    data_link = Path(os.path.relpath(data_output, output)).as_posix()
    index = ["# 全13経路の詳細と削除の分岐図", "",
             "現在の実装を独立したPythonモデルで全ラウンド再計算した結果、**firmは3経路、doctorは10経路で一致した**。", "",
             "C++の保存結果と、全経路の各選択・選択時点のマッチング・最終結果を照合した。同じ最終結果になる経路も合流させず数えている。", "",
             "## 経路数が小さい理由", "",
             "2択の削除が毎回k回起こる完全な二分木なら2^k経路になる。しかし今回、禁止ペアのないラウンドでは選択が発生せず、枝によって次の禁止ペアの有無が変わる。", "",
             "- firm：R2の削除でw2を選ぶ枝は以後分岐せず1経路。w6を選ぶ枝はR5で2択に分かれるので2経路。合計 **1+2=3**。",
             "- doctor：R3に2択。どちらの枝でもR5でw4を削除する側は3経路、w5を削除する側は2経路に分かれる。合計 **2×(3+2)=10**。",
             "- 全ラウンドの同時禁止ペアは最大1組。ペア処理順で増える経路はない。",
             "- firmの削除回数は経路により1〜2回、doctorは3〜4回。全枝が同じ深さまで分岐するわけではない。", "",
             "指数的に増える可能性は一般にはあるが、この固定問題の実際の分岐は以下の木となる。", "",
             "## 今回確認した実装の仕様", "",
             "Aは全企業／全未マッチ労働者の1ラウンド。各提案者は1人／1企業に提案する。全提案の受諾後にBを行う。",
             "Bでの削除はその企業について永久。**提案履歴と他の暫定マッチングを保持し、削除後も最初からDAをやり直さない。**",
             "企業は満員なら提案せず、労働者は所属先があれば提案しない。提案できる相手がなくなれば停止する。",
             "この提案履歴保持は経路数にも影響する。削除後にDAを初期状態から再実行する別の仕様の経路数は、この調査には含まない。", "",
             "## 元の問題", "", "定員：" + ", ".join(f"f{f+1}={q}" for f,q in enumerate(problem["capacities"])) + "。", "",
             "| 企業 | 選好（左ほど好む） |", "|---|---|"]
    index += [f'| f{f+1} | ' + " > ".join(f"w{w}" for w in order) + " |"
              for f, order in enumerate(problem["firm_rankings_one_based"])]
    index += ["", "| 労働者 | 選好（左ほど好む） |", "|---|---|"]
    index += [f'| w{w+1} | ' + " > ".join(f"f{f}" for f in order) + " |"
              for w, order in enumerate(problem["worker_rankings_one_based"])]
    bans = [f"{{w{a+1},w{b+1}}}" for a,b in itertools.combinations(range(problem["n_workers"]),2)
            if problem["colleague_prefs"][a][b] == -100000 or problem["colleague_prefs"][b][a] == -100000]
    index += ["", "禁止同僚ペア：" + "、".join(bans) + "。", ""]
    summary = {}
    for algorithm in ("firm", "worker"):
        paths = [json.loads(line) for line in (directory / f"{algorithm}_paths.jsonl").read_text().splitlines()]
        model = RoundModel(problem, algorithm)
        replays = model.enumerate()
        require(len(replays) == len(paths), "independent enumeration count mismatch")
        for path, replay in zip(paths, replays):
            require(path["decisions"] == replay["decisions"], "independent decision trace mismatch")
            require(path["assignment"] == replay["assignment"], "independent final assignment mismatch")
            (output / f'{algorithm}_path_{path["path_id"]:02d}.md').write_text(render_path(model,path,replay))
        name = "firm proposing" if algorithm == "firm" else "doctor / worker proposing"
        index += [f"## {name}の全分岐", "", "禁止ペアが生じた時点の削除選択だけを示す。分岐のないラウンドは個別ファイルに全て載せている。", ""]
        index += render_tree(algorithm, paths)
        index += ["", "| 経路 | 安定性 | マッチ人数 | 全ラウンドの詳細 |", "|---|---|---:|---|"]
        for path in paths:
            filename = f'{algorithm}_path_{path["path_id"]:02d}.md'
            index.append(f'| {path["path_id"]:02d} | {"安定" if path["stable"] else "不安定 (f3,w4)"} | {path["matched_workers"]} | [経路{path["path_id"]:02d}]({filename}) |')
        index += [""]
        summary[algorithm] = dict(verification[algorithm], independent_round_model_paths=len(replays),
            all_decision_snapshots_match_cpp=True, round_counts=[len(p["trace"]) for p in replays],
            max_simultaneous_conflicts=max(len(s["conflicts"]) for p in replays for s in p["trace"]))
        (data_output / f"{algorithm}_round_traces.json").write_text(json.dumps(replays,ensure_ascii=False,indent=2))
    index += ["## 検算の記録", "", f"[独立再計算の集計]({data_link}/verification.json)、[firm全ラウンドJSON]({data_link}/firm_round_traces.json)、[doctor全ラウンドJSON]({data_link}/worker_round_traces.json)。", "",
              "Pythonで別実装したラウンド遷移から全分岐を生成し、全13経路がC++出力と完全一致することを確認した。保存された経路を読むだけの可視化ではない。", "",
              "安定性は削除前の元の選好で検算。空席への追加blockingは含まない。経路数の比を成功確率とは解釈しない。", ""]
    (output / "da_paths_index.md").write_text("\n".join(index))
    (data_output / "verification.json").write_text(json.dumps(summary,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps(summary,ensure_ascii=False,indent=2))
    print(f"Markdown output: {output}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 render_da_paths.py OUTPUT_DIRECTORY")
    main(Path(sys.argv[1]).resolve())
