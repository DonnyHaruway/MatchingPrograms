"""Independently enumerate complete-DA restarts and verify C++ before rendering.

Run from programs:
  python3 experiments/universal_replacement/iterated_docs/render_iterated_doctor_paths.py RUN_DIRECTORY
No C++ algorithm is called by this model. Standard library only.
"""

import csv
import itertools
import json
import os
import sys
from collections import deque
from pathlib import Path

NEG_INF = -100000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def team(assignment, firm):
    return [worker + 1 for worker, label in enumerate(assignment) if label == firm]


def workers_text(workers):
    return "{" + ", ".join(f"w{worker}" for worker in workers) + "}" if workers else "∅"


def pair_text(pair):
    return f'f{pair["firm"]}: {{w{pair["left"]}, w{pair["right"]}}}'


def deletion_text(deleted):
    return "、".join(f"(f{firm},w{worker})" for firm, worker in sorted(deleted)) or "なし"


class Model:
    def __init__(self, problem):
        self.problem = problem
        self.wp = problem["agent_prefs"]
        self.fp = problem["firm_prefs"]
        self.cp = problem["colleague_prefs"]
        self.cap = problem["capacities"]
        self.w, self.f = problem["n_workers"], problem["n_firms"]
        self.side = "doctor"
        self.index_name = "da_paths_index.md"
        # The screenshot's acceptable rankings are strict, so DA ordering has no ties.
        for rankings in (problem["worker_rankings_one_based"], problem["firm_rankings_one_based"]):
            require(all(len(row) == len(set(row)) for row in rankings), "duplicate ranking label")
        for scores in self.wp + self.fp:
            accepted = [score for score in scores if score >= 0]
            require(len(accepted) == len(set(accepted)), "this profile must have strict preferences")
        self.orders = [sorted(range(self.f), key=lambda firm: -row[firm]) for row in self.wp]

    def compatible(self, members):
        return all(self.cp[left - 1][right - 1] != NEG_INF
                   and self.cp[right - 1][left - 1] != NEG_INF
                   for left, right in itertools.combinations(members, 2))

    def conflicts(self, assignment):
        return [{"firm": firm, "left": left, "right": right}
                for firm in range(1, self.f + 1)
                for left, right in itertools.combinations(team(assignment, firm), 2)
                if not self.compatible([left, right])]

    def full_doctor_da(self, deleted):
        assignment, cursor = [0] * self.w, [0] * self.w
        members = [[] for _ in range(self.f)]
        queue = deque(range(self.w))
        proposals = []
        while queue:
            worker = queue.popleft()
            if cursor[worker] == self.f:
                continue
            firm = self.orders[worker][cursor[worker]]
            cursor[worker] += 1
            if self.wp[worker][firm] < 0:
                continue
            displaced = None
            if (firm + 1, worker + 1) in deleted:
                reason = "拒否：この企業の選好から削除済み"
                queue.append(worker)
            elif self.cap[firm] == 0 or self.fp[firm][worker] < 0:
                reason = "拒否：定員0または企業側で受け入れ不可"
                queue.append(worker)
            elif len(members[firm]) < self.cap[firm]:
                members[firm].append(worker)
                assignment[worker] = firm + 1
                reason = "仮受諾：空席あり"
            else:
                worst = min(members[firm], key=lambda incumbent: (self.fp[firm][incumbent], incumbent))
                if self.fp[firm][worker] > self.fp[firm][worst]:
                    members[firm].remove(worst)
                    members[firm].append(worker)
                    assignment[worst] = 0
                    assignment[worker] = firm + 1
                    queue.append(worst)
                    displaced = worst + 1
                    reason = f"仮受諾：w{worst + 1}を拒否し直す"
                else:
                    reason = "拒否：満員で、最下位の在籍者より好まれない"
                    queue.append(worker)
            proposals.append({"worker": worker + 1, "firm": firm + 1, "reason": reason,
                              "displaced": displaced, "assignment_after": assignment.copy()})
            require(len(proposals) <= self.w * self.f, "DA proposal bound")
        require(all(assignment[worker - 1] != firm for firm, worker in deleted), "deleted edge matched")
        return {"assignment": assignment, "proposals": proposals,
                "conflicts": self.conflicts(assignment), "deleted_before": sorted(deleted)}

    def blockers(self, assignment):
        for firm in range(1, self.f + 1):
            members = team(assignment, firm)
            require(len(members) <= self.cap[firm - 1] and self.compatible(members), "final feasibility")
            require(all(self.wp[worker - 1][firm - 1] >= 0 and self.fp[firm - 1][worker - 1] >= 0
                        for worker in members), "final individual rationality")
        result = []
        for firm in range(1, self.f + 1):
            members = team(assignment, firm)
            for worker, current in enumerate(assignment, 1):
                if current == firm:
                    continue
                current_score = self.wp[worker - 1][current - 1] if current else 0
                if self.wp[worker - 1][firm - 1] <= current_score:
                    continue
                if len(members) < self.cap[firm - 1]:
                    if self.fp[firm - 1][worker - 1] > 0 and self.compatible(members + [worker]):
                        result.append({"firm": firm, "worker": worker})
                    continue
                if not any(self.fp[firm - 1][worker - 1] > self.fp[firm - 1][incumbent - 1]
                           for incumbent in members):
                    continue
                if all(self.compatible([other for other in members if other != removed] + [worker])
                       for removed in members):
                    result.append({"firm": firm, "worker": worker})
        return result

    def full_da(self, deleted):
        return self.full_doctor_da(deleted)

    def enumerate(self):
        paths = []

        def visit(deleted, decisions, trace):
            require(len(deleted) <= self.w * self.f, "deletion bound")
            step = self.full_da(deleted)
            iteration = len(trace) + 1
            step["iteration"] = iteration
            if not step["conflicts"]:
                blockers = self.blockers(step["assignment"])
                paths.append({"assignment": step["assignment"], "stable": not blockers,
                              "blocking_pairs": blockers, "decisions": decisions,
                              "trace": trace + [step]})
                return
            for index, pair in enumerate(step["conflicts"]):
                pair_decisions = []
                if len(step["conflicts"]) > 1:
                    pair_decisions.append({"round": iteration, "kind": "pair_order",
                        "option_count": len(step["conflicts"]), "selected_index": index,
                        "assignment_before": step["assignment"], "pairs": step["conflicts"]})
                for endpoint, removed in enumerate((pair["left"], pair["right"])):
                    edge = (pair["firm"], removed)
                    require(edge not in deleted, "deletion is not new")
                    decision = {"round": iteration, "kind": "removal_endpoint", "option_count": 2,
                        "selected_index": endpoint, "assignment_before": step["assignment"],
                        "pairs": [pair], "removed_worker": removed, "firm": pair["firm"]}
                    chosen_step = dict(step, selected_pair=pair, removed_worker=removed,
                                       deleted_after=sorted(deleted | {edge}))
                    visit(deleted | {edge}, decisions + pair_decisions + [decision], trace + [chosen_step])

        visit(frozenset(), [], [])
        return paths


def matching_table(assignment, firms):
    return ["| 企業 | 所属労働者 |", "|---|---|"] + [
        f"| f{firm} | {workers_text(team(assignment, firm))} |" for firm in range(1, firms + 1)
    ] + [f"| 未マッチ | {workers_text(team(assignment, 0))} |"]


def preference_table(model, deleted, heading="更新後の受け入れ可能な順位"):
    return [f"| 企業 | {heading} |", "|---|---|"] + [
        f"| f{firm} | " + (" > ".join(f"w{worker}" for worker in ranking
                                      if (firm, worker) not in deleted) or "なし") + " |"
        for firm, ranking in enumerate(model.problem["firm_rankings_one_based"], 1)]


def blocker_witness(model, assignment, blocker):
    firm, worker = blocker["firm"], blocker["worker"]
    members = team(assignment, firm)
    current = assignment[worker - 1]
    current_label = f"f{current}" if current else "未マッチ"
    current_score = model.wp[worker - 1][current - 1] if current else 0
    if len(members) < model.cap[firm - 1]:
        after = sorted(members + [worker])
        require(model.fp[firm - 1][worker - 1] > 0 and model.compatible(after), "vacancy witness infeasible")
        return [f"### (f{firm}, w{worker}) の空席への追加blocking", "",
                f"f{firm}には空席があり、w{worker}は現在の{current_label}よりf{firm}を好む"
                f"（{model.wp[worker - 1][firm - 1]} > {current_score}）。",
                f"企業の追加スコアは{model.fp[firm - 1][worker - 1]} > 0。追加後の{workers_text(after)}に禁止同僚ペアはない。"]
    inferior = [incumbent for incumbent in members
                if model.fp[firm - 1][worker - 1] > model.fp[firm - 1][incumbent - 1]]
    lines = [f"### (f{firm}, w{worker}) がblocking pairであること", "",
        f"1. f{firm}の在籍者は{workers_text(members)}。人数{len(members)}は定員{model.cap[firm - 1]}と等しい。",
        f"2. w{worker}は現在の{current_label}よりf{firm}を厳密に好む"
        f"（元のスコア：{model.wp[worker - 1][firm - 1]} > {current_score}）。",
        f"3. f{firm}はw{worker}を、少なくとも在籍者{workers_text(inferior)}より厳密に好む。",
        "4. **どの在籍者1人を置き換えても**、置換後の所属集合に禁止同僚ペアはない。", "",
        "| 置き換える在籍者 | 置換後の所属集合 | 同僚制約 |", "|---|---|---|"]
    for removed in members:
        after = sorted([other for other in members if other != removed] + [worker])
        require(model.compatible(after), "blocking witness infeasible")
        lines.append(f"| w{removed} | {workers_text(after)} | 禁止ペアなし |")
    lines += ["", f"したがって、元の市場では **(f{firm}, w{worker})** がuniversal-replacement blocking pairとなる。"]
    return lines


def render_path(model, path):
    identifier = path["path_id"]
    trace = path["trace"]
    status = "UR安定" if path["stable"] else "UR不安定"
    status_class = "stable" if path["stable"] else "unstable"
    blocker_label = "、".join(f'(f{pair["firm"]},w{pair["worker"]})' for pair in path["blocking_pairs"])
    history = " → ".join(f'DA{decision["round"]}: (f{decision["firm"]},w{decision["removed_worker"]})'
                         for decision in path["decisions"] if decision["kind"] == "removal_endpoint")
    lines = [f"# 再実行型{model.side} proposing DA — 経路 {identifier:02d}", "",
        f"[全経路の結論・分岐図]({model.index_name})", "",
        f'最終結果は **{status}**。{path["matched_workers"]}人がマッチし、blocking pairは **{blocker_label or "なし"}**。', "",
        f"削除履歴：{history}。DA実行回数は{len(trace)}回。", "",
        "以下の全DA・全削除・最終割当は、独立Pythonモデルと実際のC++アルゴリズムで一致した。", "",
        "## この経路の流れ", "", "```mermaid", "flowchart TD", '  start["全員未マッチ・削除なし"]']
    previous = "start"
    for step in trace:
        iteration = step["iteration"]
        conflict = "<br/>".join(pair_text(pair) for pair in step["conflicts"]) or "禁止ペアなし"
        lines += [f'  a{iteration}["DA{iteration}: 最初から終了まで実行<br/>{conflict}"]',
                  f"  {previous} --> a{iteration}"]
        previous = f"a{iteration}"
        if step["conflicts"]:
            firm, removed = step["selected_pair"]["firm"], step["removed_worker"]
            lines += [f'  b{iteration}{{"f{firm}の選好からw{removed}を削除<br/>累積保持してDAを初期化"}}',
                      f"  {previous} --> b{iteration}"]
            previous = f"b{iteration}"
    lines += [f'  done["終了: {path["matched_workers"]}人マッチ<br/>{status}: {blocker_label or "blocking pairなし"}"]',
              f"  {previous} --> done", "  classDef unstable fill:#fadddd,stroke:#b32626,color:#222;",
              "  classDef stable fill:#d9f2df,stroke:#237a3b,color:#222;",
              f"  class done {status_class}", "```", "", "## 最終マッチング", ""]
    lines += matching_table(path["assignment"], model.f)
    lines += ["", "## DA実行ごとの詳細", "",
              "DA番号は完全なDAの実行回数。内部の提案ラウンド番号ではない。毎回、全員未マッチから開始する。"]
    for step in trace:
        iteration = step["iteration"]
        lines += ["", f"### DA{iteration}", "", f'開始時の累積削除：{deletion_text(step["deleted_before"])}。', "",
                  "**A：同僚選好を無視してDAを完了**", ""]
        lines += matching_table(step["assignment"], model.f)
        explanation = ("個人はより好む企業の提案を受けると移る。削除済みの個人は企業の提案候補から除く。"
                       if model.side == "firm" else
                       "仮受諾後に別の提案で拒否される場合がある。企業からの拒否・削除済み企業への提案も記録する。")
        lines += ["", "**DA内部の全提案**", "", explanation, "",
                  "| 順番 | 提案 | その時点での処理 |", "|---|---|---|"]
        for index, proposal in enumerate(step["proposals"], 1):
            worker, firm = proposal["worker"], proposal["firm"]
            offer = f"f{firm} → w{worker}" if model.side == "firm" else f"w{worker} → f{firm}"
            lines.append(f'| {index} | {offer} | {proposal["reason"]} |')
        stop_reason = ("空席があって提案可能な個人が残る企業がなくなると、このDAは終了する。"
                       if model.side == "firm" else
                       "受け入れ可能な企業への提案が尽きるか、全員が仮受諾を得ると、このDAは終了する。")
        lines += ["", stop_reason, "",
                  "**B：完了したDAの禁止ペアを確認**", ""]
        if not step["conflicts"]:
            lines += ["禁止ペアはない。このマッチングを返してアルゴリズムを終了する。"]
        else:
            pair = step["selected_pair"]
            lines += [f'- {pair_text(conflict)}' for conflict in step["conflicts"]]
            lines += ["", f'選択した組は{pair_text(pair)}。削除の2択はw{pair["left"]}／w{pair["right"]}。', "",
                      f'この経路では **f{pair["firm"]}からw{step["removed_worker"]}を削除**。', "",
                      "**C：更新した企業選好を累積保持**", "",
                      f'累積削除：{deletion_text(step["deleted_after"])}。', ""]
            lines += preference_table(model, {tuple(edge) for edge in step["deleted_after"]})
            lines += ["", "個人の企業選好・同僚選好は元のまま。全マッチと全提案履歴を初期化して次のDAへ進む。"]
    lines += ["", "## 元の選好による安定性判定", "",
              "最終マッチングは定員を守り、個人合理的で、禁止同僚ペアもない。", ""]
    if path["stable"]:
        lines += ["削除前の元の選好で全候補を調べても、universal-replacement blocking pairはない。", ""]
        if hasattr(model, "audit_table"):
            lines += model.audit_table(path["assignment"]) + [""]
    else:
        lines += ["ただし、削除前の元の選好で評価すると以下のblocking pairがある。", ""]
    for blocker in path["blocking_pairs"]:
        lines += blocker_witness(model, path["assignment"], blocker) + [""]
    lines += ["削除によって企業の候補から除外された個人との関係も、元の市場に戻して判定する。", ""]
    return "\n".join(lines)


def render_index(model, paths, relative_run, verification):
    lines = ["# 再実行型doctor proposing DA：全経路探索と仮説の検証", "",
        "**この固定問題では仮説が成立する。全4経路を探索した結果、UR安定な終点は0件だった。**", "",
        "対象は `iterated_doctor_proposing_DA_algorithm`。DAを終了まで実行し、拒否ペアを1組選んで片方を削除し、"
        "更新した選好でDAを最初から再実行する方式を検証した。", "",
        "## 集計", "", "| 項目 | 結果 |", "|---|---:|",
        f'| 全選択経路 | {len(paths)} |',
        f'| UR安定な経路 | {verification["stable_paths"]} |',
        f'| UR不安定な経路 | {len(paths) - verification["stable_paths"]} |',
        f'| 異なる最終マッチング | {verification["distinct_matchings"]} |',
        f'| 最大同時禁止ペア | {verification["max_simultaneous_conflicts"]} |',
        "| 経路ごとの削除回数 | 1〜3 |", "| 経路ごとの完全DA実行回数 | 2〜4 |", "",
        "本資料でのUR安定は、定員・個人合理性・同僚制約を満たし、元の選好に対する"
        "universal-replacement blocking pairと空席への追加blockingがないこと。", "",
        "## 全経路の分岐図", "",
        "各分岐の前には、同僚選好を無視したDAを**最後まで**実行する。各矢印の後に削除を累積し、"
        "全員未マッチ・提案履歴なしから再実行する。", "", "```mermaid", "flowchart TD"]
    next_id = itertools.count()

    def tree(subset, depth):
        node = f"n{next(next_id)}"
        decision = subset[0]["decisions"][depth] if depth < len(subset[0]["decisions"]) else None
        if decision is None:
            require(len(subset) == 1, "duplicate terminal decision history")
            path = subset[0]
            blockers = "、".join(f'(f{pair["firm"]},w{pair["worker"]})' for pair in path["blocking_pairs"])
            lines.append(f'  {node}["経路{path["path_id"]:02d}: {path["matched_workers"]}人マッチ<br/>UR不安定: {blockers}"]')
            lines.append(f"  class {node} unstable")
            lines.append(f'  click {node} "doctor_path_{path["path_id"]:02d}.md"')
            return node
        require(decision["kind"] == "removal_endpoint", "profile unexpectedly has simultaneous conflicts")
        pair = decision["pairs"][0]
        lines.append(f'  {node}{{"DA{decision["round"]}完了<br/>{pair_text(pair)}"}}')
        for selected in range(decision["option_count"]):
            child_paths = [path for path in subset if path["decisions"][depth]["selected_index"] == selected]
            require(child_paths, "missing branch")
            child = tree(child_paths, depth + 1)
            removed = pair["left"] if selected == 0 else pair["right"]
            lines.append(f'  {node} -->|"f{pair["firm"]}からw{removed}を削除"| {child}')
        return node

    tree(paths, 0)
    lines += ["  classDef unstable fill:#fadddd,stroke:#b32626,color:#222;", "```", "",
              "経路数は **1 + 1 + 2 = 4**。最初にw2を削除した枝は直ちに終点へ進む。"
              "w6を削除した枝は次の禁止ペアで2分岐し、その片方だけがさらに2分岐する。", "",
              "## 全経路の終点", "",
              "| 経路 | 累積削除の順序 | f1 | f2 | f3 | f4 | f5 | 未マッチ | UR blocking pair | 詳細 |",
              "|---|---|---|---|---|---|---|---|---|---|"]
    for path in paths:
        deletion = " → ".join(f'(f{decision["firm"]},w{decision["removed_worker"]})'
                              for decision in path["decisions"] if decision["kind"] == "removal_endpoint")
        labels = [workers_text(team(path["assignment"], firm)) for firm in [1, 2, 3, 4, 5, 0]]
        blockers = "、".join(f'(f{pair["firm"]},w{pair["worker"]})' for pair in path["blocking_pairs"])
        lines.append(f'| {path["path_id"]:02d} | {deletion} | ' + " | ".join(labels)
                     + f' | {blockers} | [経路{path["path_id"]:02d}](doctor_path_{path["path_id"]:02d}.md) |')
    lines += ["", "8人全員がマッチする経路02・04でもblocking pairがある。各経路の詳細に、"
              "候補の企業選好、企業側の比較、全在籍者についての置換後集合を載せた。", "",
              "## なぜ禁止ペアがなくても不安定なのか", "",
              "終了条件は同じ企業に禁止同僚ペアがないこと。これは、元の市場でのUR安定性を保証しない。"
              "4経路すべてで、残ったblocking pairは過去に削除した企業・個人の組そのものだった。"
              "削除によってDAの候補から外されても、元の企業選好では依然として好まれており、"
              "終了時の在籍者となら禁止同僚ペアを作らずに置換できる。", "",
              "## 元の問題", "", "定員は `(2,2,2,2,1)`。順位表にない相手は受け入れ不可。", ""]
    lines += preference_table(model, set(), "元の受け入れ可能な順位")
    lines += ["", "| 個人 | 企業への順位 |", "|---|---|"]
    lines += [f"| w{worker} | " + " > ".join(f"f{firm}" for firm in ranking) + " |"
              for worker, ranking in enumerate(model.problem["worker_rankings_one_based"], 1)]
    forbidden = [(left, right) for left in range(1, model.w + 1) for right in range(left + 1, model.w + 1)
                 if not model.compatible([left, right])]
    lines += ["", "禁止同僚ペア（対称）：" + "、".join(workers_text(pair) for pair in forbidden) + "。", "",
              "## 安定性の定義", "",
              "企業fと現在そこにいない個人wが、以下をすべて満たす場合にUR blocking pairとする。", "",
              "1. 企業fが満員である。",
              "2. 個人wが現在の企業よりfを厳密に好む（未マッチの評価は0）。",
              "3. 企業fが少なくとも1人の在籍者よりwを厳密に好む。",
              "4. どの在籍者1人をwに置き換えても、置換後に禁止同僚ペアがない。", "",
              "判定には、削除を反映した一時的な企業選好ではなく、**削除前の元の企業選好**を用いた。", "",
              "## 全探索と独立検算", "",
              "C++の `enumerate_choice_paths` が実際のアルゴリズムを再生し、未指定の意思決定に達するたびに"
              "全選択肢へ分岐する。ペアが複数なら全ペア、削除対象は両端点を列挙する。"
              "seedのサンプリングや、安定解が見つかった時点での打ち切りは行わない。", "",
              "本問題では各DA完了時の禁止ペアは高々1組なので、全分岐は削除対象の2択。"
              "DA内の受け入れ可能な順位は厳密であり、提案順の追加の乱択はない。", "",
              "Pythonは元の問題から標準DAと削除の分岐を別実装し、C++を呼ばずに全4経路を生成した。"
              "各選択のDA番号・選択肢・削除前の割当・削除対象・終点をC++ JSONLと完全照合し、"
              "元の選好からblocking pairも再計算した。CSVとの一致も確認した。", "",
              "## 仮説が成立する範囲", "",
              "確認したのは、添付画像の固定された5企業・8個人問題と、今回の再実行型doctor提案DA。"
              "このアルゴリズムのあらゆる削除選択経路でUR安定な結果に到達できない。", "",
              "これは全市場に対する一般定理や、この市場に安定マッチングが存在しないという主張ではない。"
              "また、各提案ラウンド後に削除し提案履歴を保持する既存の `modified_worker_proposing_DA_algorithm` は"
              "別の方式であり、[既存の調査](../modified_docs/da_paths_index.md)では10経路中8経路がUR安定だった。", "",
              "## 保存した検証データ", "",
              f'- [C++集計]({relative_run}/summary.txt)',
              f'- [元の全選好]({relative_run}/problem.json)',
              f'- [C++全経路JSONL]({relative_run}/iterated_doctor_paths.jsonl)',
              f'- [全経路CSV]({relative_run}/iterated_doctor_paths.csv)',
              f'- [独立検算の集計]({relative_run}/verification.json)',
              f'- [全DA・全提案の独立計算ログ]({relative_run}/doctor_da_traces.json)', "",
              "再実行方法は [README](README.md) を参照。", ""]
    return "\n".join(lines)


def verify_paths(directory, model, algorithm):
    independently_computed = model.enumerate()
    cpp_paths = [json.loads(line) for line in (directory / f"{algorithm}_paths.jsonl").read_text().splitlines()]
    require(len(cpp_paths) == len(independently_computed), "independent enumeration path count mismatch")
    with (directory / f"{algorithm}_paths.csv").open() as source:
        csv_paths = list(csv.DictReader(source))
    require(len(csv_paths) == len(cpp_paths), "CSV path count")
    verified = []
    for identifier, (cpp, replay, row) in enumerate(zip(cpp_paths, independently_computed, csv_paths), 1):
        require(cpp["algorithm"] == algorithm, "wrong algorithm")
        require(cpp["path_id"] == identifier == int(row["path_id"]), "path ID")
        for key in ("assignment", "decisions", "blocking_pairs", "stable"):
            require(cpp[key] == replay[key], f"independent path {identifier}: {key}")
        matched = sum(label != 0 for label in replay["assignment"])
        require(matched == cpp["matched_workers"] == int(row["matched_workers"]), "matched count")
        require(row["stable"] == str(cpp["stable"]).lower(), "CSV stability")
        require(int(row["outcome_id"]) == cpp["outcome_id"], "CSV outcome ID")
        require([0 if row[f"w{worker}"] == "unmatched" else int(row[f"w{worker}"][1:])
                 for worker in range(1, model.w + 1)] == cpp["assignment"], "CSV assignment")
        require(row["blocking_pairs"] == ";".join(f'f{pair["firm"]}:w{pair["worker"]}'
                 for pair in cpp["blocking_pairs"]), "CSV blockers")
        require(row["deletions"] == ";".join(f'r{d["round"]}:f{d["firm"]}:w{d["removed_worker"]}'
                 for d in cpp["decisions"] if d["kind"] == "removal_endpoint"), "CSV deletions")
        verified.append(dict(cpp, trace=replay["trace"]))
    stable_paths = sum(path["stable"] for path in verified)
    verification = {"algorithm": algorithm, "paths": len(verified), "stable_paths": stable_paths,
        "distinct_matchings": len({tuple(path["assignment"]) for path in verified}),
        "max_simultaneous_conflicts": max(len(step["conflicts"]) for path in verified for step in path["trace"]),
        "all_paths_independently_enumerated": True, "cpp_decisions_and_assignments_match": True,
        "original_preference_blockers_match": True, "csv_jsonl_match": True,
        "hypothesis_holds_for_this_profile": stable_paths == 0}
    return verified, verification


def main(directory):
    problem = json.loads((directory / "problem.json").read_text())
    model = Model(problem)
    verified, verification = verify_paths(directory, model, "iterated_doctor")
    stable_paths = verification["stable_paths"]
    require(len(verified) == 4 and stable_paths == 0
            and verification["distinct_matchings"] == 4
            and verification["max_simultaneous_conflicts"] == 1,
            "fixed-profile report assumptions have changed; update the report conclusions")
    # Verify the whole dataset before creating reports. Raw C++ output is preserved.
    (directory / "verification.json").write_text(json.dumps(verification, indent=2) + "\n")
    (directory / "doctor_da_traces.json").write_text(json.dumps(verified, ensure_ascii=False, indent=2) + "\n")
    docs = Path(__file__).resolve().parent
    relative_run = Path(os.path.relpath(directory, docs)).as_posix()
    (docs / "da_paths_index.md").write_text(render_index(model, verified, relative_run, verification))
    for path in verified:
        (docs / f'doctor_path_{path["path_id"]:02d}.md').write_text(render_path(model, path))
    print(json.dumps(verification, indent=2))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: render_iterated_doctor_paths.py RUN_DIRECTORY")
    main(Path(sys.argv[1]).resolve())
