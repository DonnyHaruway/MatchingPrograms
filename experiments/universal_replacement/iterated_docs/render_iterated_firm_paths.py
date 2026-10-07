"""Independently enumerate complete firm DA restarts, verify C++, and render.

Standard library only. The shared verifier does not call C++.
"""

import itertools
import json
import os
import sys
from collections import deque
from pathlib import Path

from render_iterated_doctor_paths import (
    Model, deletion_text, pair_text, preference_table, render_path,
    require, team, verify_paths, workers_text,
)


class FirmModel(Model):
    def __init__(self, problem):
        super().__init__(problem)
        self.side = "firm"
        self.index_name = "firm_da_paths_index.md"

    def full_da(self, deleted):
        # Rebuild all proposal orders, matches, and cursors after every deletion.
        orders = [sorted((worker for worker in range(self.w)
                          if self.fp[firm][worker] >= 0 and (firm + 1, worker + 1) not in deleted),
                         key=lambda worker: (-self.fp[firm][worker], worker))
                  for firm in range(self.f)]
        assignment, cursor = [0] * self.w, [0] * self.f
        members = [set() for _ in range(self.f)]
        queue = deque(firm for firm in range(self.f) if self.cap[firm] > 0)
        proposals = []
        while queue:
            firm = queue.popleft()
            while len(members[firm]) < self.cap[firm] and cursor[firm] < len(orders[firm]):
                worker = orders[firm][cursor[firm]]
                cursor[firm] += 1
                old_firm = assignment[worker]
                if self.wp[worker][firm] < 0:
                    reason = "拒否：個人側で受け入れ不可"
                elif old_firm and self.wp[worker][firm] <= self.wp[worker][old_firm - 1]:
                    reason = f"拒否：現在のf{old_firm}を維持"
                else:
                    if old_firm:
                        old = old_firm - 1
                        if len(members[old]) == self.cap[old]:
                            queue.append(old)
                        members[old].remove(worker)
                        reason = f"仮受諾：f{old_firm}からf{firm + 1}へ移動"
                    else:
                        reason = "仮受諾：未マッチから所属"
                    members[firm].add(worker)
                    assignment[worker] = firm + 1
                proposals.append({"worker": worker + 1, "firm": firm + 1, "reason": reason,
                                  "previous_firm": old_firm, "assignment_after": assignment.copy()})
                require(len(proposals) <= self.w * self.f, "firm DA proposal bound")
        require(all(assignment[worker - 1] != firm for firm, worker in deleted), "deleted edge matched")
        return {"assignment": assignment, "proposals": proposals,
                "conflicts": self.conflicts(assignment), "deleted_before": sorted(deleted)}

    def audit_table(self, assignment):
        lines = ["### 全企業・全個人の候補検査", "",
            "各企業の在籍者を除く全候補を、削除前の元の選好で検査した。"
            "各行に、blocking pairの必要条件を満たさない理由を1つ示す。"
            "削除済みの企業・個人の組も除外せず検査する。", "",
            "| 候補 | 現在の所属 | blocking pairにならない理由 |", "|---|---|---|"]
        for firm in range(1, self.f + 1):
            members = team(assignment, firm)
            for worker, current in enumerate(assignment, 1):
                if current == firm:
                    continue
                current_label = f"f{current}" if current else "未マッチ"
                current_score = self.wp[worker - 1][current - 1] if current else 0
                if len(members) < self.cap[firm - 1]:
                    reason = f"企業が満員ではない（{len(members)}/{self.cap[firm - 1]}人）"
                elif self.wp[worker - 1][firm - 1] <= current_score:
                    reason = "個人がこの企業を現在の所属より厳密に好まない（受け入れ不可を含む）"
                elif not any(self.fp[firm - 1][worker - 1] > self.fp[firm - 1][incumbent - 1]
                             for incumbent in members):
                    reason = "企業が候補をどの在籍者よりも厳密に好まない"
                else:
                    failed = next((removed for removed in members
                                   if not self.compatible([other for other in members if other != removed] + [worker])), None)
                    require(failed is not None, "unexpected UR blocking pair in stable audit")
                    after = sorted([other for other in members if other != failed] + [worker])
                    forbidden = [pair for pair in itertools.combinations(after, 2) if not self.compatible(pair)]
                    reason = f"w{failed}の置換で禁止ペア" + "、".join(workers_text(pair) for pair in forbidden) + "が生じる"
                lines.append(f"| (f{firm},w{worker}) | {current_label} | {reason} |")
        require(not self.blockers(assignment), "stable audit disagrees with independent blocker enumeration")
        return lines


def render_index(model, paths, relative_run, verification):
    lines = ["# 再実行型firm proposing DA：全経路探索", "",
        "**この固定問題では全3経路がUR安定だった。うち経路02は8人全員がマッチする。**", "",
        "対象は `iterated_firm_proposing_DA_algorithm`。同僚選好を無視してfirm proposing DAを終了まで実行し、"
        "禁止ペアを1組選び、その片方を該当企業の選好から削除する。削除を累積保持してDAを最初から再実行する。", "",
        "判定は削除前の元の選好に対するuniversal-replacement安定性。空席への追加blockingは含めない。", "",
        "## 集計とdoctor版との比較", "", "| 項目 | firm proposing | doctor proposing |", "|---|---:|---:|",
        f'| 全経路 | {len(paths)} | 4 |',
        f'| UR安定な経路 | {verification["stable_paths"]} | 0 |',
        "| UR不安定な経路 | 0 | 4 |", "| 異なる最終マッチング | 3 | 4 |",
        "| 全員マッチかつUR安定な経路 | 1 | 0 |",
        "| 最大同時禁止ペア | 1 | 1 |", "| 削除回数 | 1〜2 | 1〜3 |",
        "| 完全なDAの実行回数 | 2〜3 | 2〜4 |", "",
        "doctor版の結果は [全4経路の調査](da_paths_index.md) を参照。"
        "今回の固定問題では「doctor版は全経路でUR不安定、firm版は全経路でUR安定」が確認できた。", "",
        "## 全経路のMermaid分岐図", "",
        "DA番号は完全なDAの実行回数。各削除後に全員未マッチ・提案履歴なしから再実行する。", "",
        "```mermaid", "flowchart TD"]
    identifiers = itertools.count()

    def tree(subset, depth):
        node = f"n{next(identifiers)}"
        if depth == len(subset[0]["decisions"]):
            require(len(subset) == 1, "duplicate terminal history")
            path = subset[0]
            lines.extend([f'  {node}["経路{path["path_id"]:02d}: UR安定<br/>{path["matched_workers"]}人マッチ・blocking pairなし"]',
                          f"  class {node} stable", f'  click {node} "firm_path_{path["path_id"]:02d}.md"'])
            return node
        decision = subset[0]["decisions"][depth]
        require(decision["kind"] == "removal_endpoint", "fixed profile has changed")
        pair = decision["pairs"][0]
        lines.append(f'  {node}{{"DA{decision["round"]}完了<br/>{pair_text(pair)}"}}')
        for endpoint in range(2):
            child_paths = [path for path in subset if path["decisions"][depth]["selected_index"] == endpoint]
            require(child_paths, "missing endpoint branch")
            child = tree(child_paths, depth + 1)
            removed = pair["left"] if endpoint == 0 else pair["right"]
            lines.append(f'  {node} -->|"f{pair["firm"]}からw{removed}を削除"| {child}')
        return node

    tree(paths, 0)
    lines += ["  classDef stable fill:#d9f2df,stroke:#237a3b,color:#222;", "```", "",
              "最初の禁止ペアはf1の{w2,w6}。w2を削除した枝は次のDAで停止する。"
              "w6を削除した枝だけがf3の{w4,w5}でさらに2分岐するため、経路数は **1 + 2 = 3**。", "",
              "## 全経路の終点", "",
              "| 経路 | 削除の順序 | f1 | f2 | f3 | f4 | f5 | 未マッチ | マッチ人数 | UR blocking pair | 詳細 |",
              "|---|---|---|---|---|---|---|---|---:|---|---|"]
    for path in paths:
        history = " → ".join(f'(f{d["firm"]},w{d["removed_worker"]})' for d in path["decisions"]
                              if d["kind"] == "removal_endpoint")
        labels = [workers_text(team(path["assignment"], firm)) for firm in [1, 2, 3, 4, 5, 0]]
        lines.append(f'| {path["path_id"]:02d} | {history} | ' + " | ".join(labels)
                     + f' | {path["matched_workers"]} | なし | [経路{path["path_id"]:02d}](firm_path_{path["path_id"]:02d}.md) |')
    lines += ["", "## 元の問題", "", "定員は `(2,2,2,2,1)`。順位表にない相手は受け入れ不可。", ""]
    lines += preference_table(model, set(), "元の受け入れ可能な順位")
    lines += ["", "| 個人 | 企業への順位 |", "|---|---|"]
    lines += [f"| w{worker} | " + " > ".join(f"f{firm}" for firm in ranking) + " |"
              for worker, ranking in enumerate(model.problem["worker_rankings_one_based"], 1)]
    forbidden = [pair for pair in itertools.combinations(range(1, model.w + 1), 2) if not model.compatible(pair)]
    lines += ["", "禁止同僚ペア（対称）：" + "、".join(workers_text(pair) for pair in forbidden) + "。", "",
              "## 安定性の確認方法", "",
              "各終点が定員・個人合理性・禁止同僚ペアの制約を満たすことを確認し、"
              "全企業について、現在そこにいない全個人を元の選好で検査した。"
              "次の4条件を同時に満たす候補は、3経路とも存在しなかった。", "",
              "1. 企業が満員である。", "2. 個人が現在の企業よりその企業を厳密に好む（未マッチの評価は0）。",
              "3. 企業が少なくとも1人の在籍者よりその個人を厳密に好む。",
              "4. どの在籍者1人をその個人に置き換えても禁止同僚ペアがない。", "",
              "各経路の資料に全候補の検査表を載せた。企業の選好から削除された個人も検査から除いていない。", "",
              "## 全探索と独立検算", "",
              "C++は実際のアルゴリズムに `DARandomChoiceSelector` を渡し、未指定の選択に達するたび"
              "全選択肢を分岐させる。複数の禁止ペアがあれば全ペアを、削除対象は両端点を探索する。"
              "seedのサンプリングや、安定解発見による早期終了は行わない。", "",
              "独立Pythonモデルは、企業が順位の高い個人から空席が埋まるまで提案する通常DAを別実装した。"
              "個人がより好む企業に移ると旧企業の提案を再開し、各削除後に全状態を初期化する。"
              "そのモデルから削除の全分岐を生成し、C++の全選択履歴・削除前の割当・最終割当・"
              "blocking pair・CSVを照合した。3経路すべてで一致した。", "",
              "## 結論が成立する範囲", "",
              "対象は画像の固定された5企業・8個人問題と、今回の再実行型firm提案DA。"
              "この問題については、どの削除対象を選んでもUR安定な結果に到達する。"
              "全市場について同じ性質が成り立つという一般定理ではない。", "",
              "各提案ラウンド後に削除して履歴を保持する方式は [従来の調査](../modified_docs/da_paths_index.md) を参照。"
              "従来のfirm版も3経路がUR安定だったが、今回の経路02は8人全員がマッチする点が異なる。", "",
              "## 保存したデータ", "",
              f'- [C++集計]({relative_run}/summary.txt)', f'- [元の全選好]({relative_run}/problem.json)',
              f'- [C++全経路JSONL]({relative_run}/iterated_firm_paths.jsonl)',
              f'- [全経路CSV]({relative_run}/iterated_firm_paths.csv)',
              f'- [独立検算の集計]({relative_run}/verification.json)',
              f'- [全DA・全提案の独立計算ログ]({relative_run}/firm_da_traces.json)', "",
              "再実行方法は [README](README.md) を参照。", ""]
    return "\n".join(lines)


def main(directory):
    model = FirmModel(json.loads((directory / "problem.json").read_text()))
    paths, verification = verify_paths(directory, model, "iterated_firm")
    verification.pop("hypothesis_holds_for_this_profile")
    verification.update(has_stable_path=any(path["stable"] for path in paths),
                        all_paths_stable=all(path["stable"] for path in paths),
                        fully_matched_stable_paths=sum(path["stable"] and path["matched_workers"] == model.w
                                                       for path in paths))
    require(len(paths) == 3 and verification["stable_paths"] == 3
            and verification["distinct_matchings"] == 3 and verification["fully_matched_stable_paths"] == 1
            and verification["max_simultaneous_conflicts"] == 1,
            "fixed-profile report assumptions have changed; update the conclusions")
    # Validate the doctor comparison against its recorded independent result.
    docs = Path(__file__).resolve().parent
    doctor_run = docs / "recorded_results/iterated_doctor_paths_1791367650792"
    doctor_model = Model(json.loads((doctor_run / "problem.json").read_text()))
    require(doctor_model.problem == model.problem, "comparison uses different markets")
    _, doctor_verification = verify_paths(doctor_run, doctor_model, "iterated_doctor")
    require(doctor_verification["paths"] == 4 and doctor_verification["stable_paths"] == 0,
            "doctor comparison result has changed")
    for path in paths:
        model.audit_table(path["assignment"])
    (directory / "verification.json").write_text(json.dumps(verification, indent=2) + "\n")
    (directory / "firm_da_traces.json").write_text(json.dumps(paths, ensure_ascii=False, indent=2) + "\n")
    relative_run = Path(os.path.relpath(directory, docs)).as_posix()
    (docs / "firm_da_paths_index.md").write_text(render_index(model, paths, relative_run, verification))
    for path in paths:
        (docs / f'firm_path_{path["path_id"]:02d}.md').write_text(render_path(model, path))
    print(json.dumps(verification, indent=2))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: render_iterated_firm_paths.py RUN_DIRECTORY")
    main(Path(sys.argv[1]).resolve())
