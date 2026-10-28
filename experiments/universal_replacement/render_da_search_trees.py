"""Verify C++ search trees with independent Python DAs, then render Markdown/Mermaid.

Usage: python3 experiments/universal_replacement/render_da_search_trees.py RUN_DIRECTORY
Standard library only. Does not modify historical iterated_docs reports.
"""

import json
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "iterated_docs"))
from render_iterated_doctor_paths import Model, require, team, workers_text
from render_iterated_firm_paths import FirmModel


def blocker_pairs(model, assignment, deleted):
    """Evaluate all candidates, including infeasible intermediate DA assignments."""
    result = []
    for firm in range(1, model.f + 1):
        members = team(assignment, firm)
        for worker, current in enumerate(assignment, 1):
            if current == firm or (firm, worker) in deleted:
                continue
            current_score = model.wp[worker - 1][current - 1] if current else 0
            if model.wp[worker - 1][firm - 1] <= current_score:
                continue
            score = model.fp[firm - 1][worker - 1]
            if len(members) < model.cap[firm - 1]:
                blocking = score > 0 and model.compatible(members + [worker])
            else:
                blocking = (any(score > model.fp[firm - 1][other - 1] for other in members)
                            and all(model.compatible([other for other in members if other != removed] + [worker])
                                    for removed in members))
            if blocking:
                result.append({"firm": firm, "worker": worker})
    return result


def branch_history(tree, leaf):
    incoming = {branch["to"]: branch for branch in tree["branches"] if branch["to"] is not None}
    history = []
    while leaf != 0:
        branch = incoming[leaf]
        history.append(branch)
        leaf = branch["from"]
    return list(reversed(history))


def path_signature(tree, leaf):
    return (tree["nodes"][leaf]["assignment"], [
        (b["firm"], tree["nodes"][b["from"]]["conflicts"][b["conflict_index"]]["left"],
         tree["nodes"][b["from"]]["conflicts"][b["conflict_index"]]["right"], b["removed_worker"])
        for b in branch_history(tree, leaf)])


def verify(tree, model):
    nodes, branches = tree["nodes"], tree["branches"]
    require(nodes and nodes[0]["deletions"] == [], "missing root")
    require([node["id"] for node in nodes] == list(range(len(nodes))), "node IDs")
    incoming = []
    for node in nodes:
        deleted = {(d["firm"], d["worker"]) for d in node["deletions"]}
        require(len(deleted) == len(node["deletions"]), "deletion repeated on a path")
        replay = model.full_da(frozenset(deleted))
        require(node["assignment"] == replay["assignment"], "independent DA assignment mismatch")
        require(node["conflicts"] == replay["conflicts"], "independent conflict mismatch")
        require(node["da_run"] == len(deleted) + 1, "DA run number")
        original = blocker_pairs(model, node["assignment"], set())
        reduced = blocker_pairs(model, node["assignment"], deleted)
        require(node["original_blocking_pairs"] == original, "original blockers mismatch")
        require(node["original_ur_stable"] == (not node["conflicts"] and not original), "original UR mismatch")
        require(node["reduced_ur_stable"] == (not node["conflicts"] and not reduced), "reduced UR mismatch")
        outgoing = [branch for branch in branches if branch["from"] == node["id"]]
        expected = [(index, removed) for index, pair in enumerate(node["conflicts"])
                    for removed in (pair["left"], pair["right"])]
        require([(b["conflict_index"], b["removed_worker"]) for b in outgoing] == expected,
                "must preserve all 2 * conflicts branches and their order")
        executed = [b for b in outgoing if b["to"] is not None]
        require(len(executed) == (len(expected) if tree["mode"] == "recursive" else int(bool(expected))),
                "incorrect number of executed branches")
        for branch in outgoing:
            pair = node["conflicts"][branch["conflict_index"]]
            require(branch["firm"] == pair["firm"], "branch firm")
            removed = branch["removed_worker"]
            retained = pair["right"] if removed == pair["left"] else pair["left"]
            require(branch["retained_worker"] == retained, "retained worker")
            require(branch["removed_before"] == branch["retained_before"] == pair["firm"], "pair is not matched")
            if branch["to"] is None:
                require(all(branch[key] is None for key in ("removed_after", "retained_after", "retained_moved")),
                        "unexecuted branch must not claim an outcome")
                continue
            require(node["id"] < branch["to"] < len(nodes), "child ID")
            incoming.append(branch["to"])
            child = nodes[branch["to"]]
            require(child["deletions"] == node["deletions"] + [{"firm": pair["firm"], "worker": removed}],
                    "child deletion history / sibling state mismatch")
            require(branch["removed_after"] == child["assignment"][removed - 1], "removed move")
            require(branch["retained_after"] == child["assignment"][retained - 1], "retained move")
            require(branch["retained_moved"] == (branch["retained_after"] != pair["firm"]), "retained moved flag")
    require(sorted(incoming) == list(range(1, len(nodes))), "tree must reach each non-root node exactly once")
    require(tree["leaves"] == [node["id"] for node in nodes if not node["conflicts"]], "leaf IDs")
    independently_enumerated = model.enumerate()
    signatures = [(p["assignment"], [(s["selected_pair"]["firm"], s["selected_pair"]["left"],
                    s["selected_pair"]["right"], s["removed_worker"]) for s in p["trace"][:-1]])
                  for p in independently_enumerated]
    actual = [path_signature(tree, leaf) for leaf in tree["leaves"]]
    if tree["mode"] == "recursive":
        require(actual == signatures, "independent exhaustive paths / duplicate histories mismatch")
    else:
        require(len(actual) == 1 and actual[0] in signatures, "iterated path missing from full enumeration")
    return {"algorithm": tree["algorithm"], "nodes": len(nodes), "branches": len(branches),
            "leaves": len(actual), "original_ur_stable_leaves": sum(nodes[i]["original_ur_stable"] for i in tree["leaves"]),
            "independent_da_and_blockers_match": True, "independent_deletion_paths_match": True}


def employer(label):
    return f"f{label}" if label else "未マッチ"


def pair_text(pair):
    return f'f{pair["firm"]}: (w{pair["left"]},w{pair["right"]})'


def blockers_text(pairs):
    return ", ".join(f'(f{p["firm"]},w{p["worker"]})' for p in pairs) or "なし"


def matching_diagram(node, model):
    identifier = node["id"]
    prefix = f"n{identifier}"
    if node["conflicts"]:
        status = "違反: " + " / ".join(pair_text(p) for p in node["conflicts"])
        color = "#fff7e6,stroke:#a66b12"
    else:
        status = "終点 / 元の問題でUR" + ("安定" if node["original_ur_stable"] else "不安定")
        color = "#e8f5e9,stroke:#267d3d" if node["original_ur_stable"] else "#ffebee,stroke:#b32626"
    lines = [f'  subgraph {prefix}["N{identifier} / DA{node["da_run"]}<br/>{status}"]', "    direction TB"]
    for firm in range(1, model.f + 1):
        occupied = len(team(node["assignment"], firm))
        lines.append(f'    {prefix}f{firm}["f{firm} ({occupied}/{model.cap[firm - 1]})"]')
    for worker, firm in enumerate(node["assignment"], 1):
        lines.append(f'    {prefix}w{worker}(("w{worker}"))')
        if not firm:
            lines.append(f"    class {prefix}w{worker} unmatched")
    # Emit company components in company order. An invisible child keeps an
    # empty company on the employer row instead of among the worker nodes.
    for firm in range(1, model.f + 1):
        members = team(node["assignment"], firm)
        for worker in members:
            lines.append(f"    {prefix}f{firm} --- {prefix}w{worker}")
        if not members:
            lines += [f'    {prefix}empty{firm}((" ")):::placeholder',
                      f"    {prefix}f{firm} ~~~ {prefix}empty{firm}"]
    lines += ["  end", f"  style {prefix} fill:{color},stroke-width:2px"]
    return lines


def mermaid(tree, model):
    lines = ["```mermaid", "flowchart TB", "  classDef unmatched fill:#f3f4f6,stroke:#777,stroke-dasharray:4 3;",
             "  classDef placeholder fill:transparent,stroke:transparent,color:transparent;",
             "  classDef unexecuted fill:#fafafa,stroke:#999,color:#666,stroke-dasharray:4 3;"]
    for node in tree["nodes"]:
        lines += matching_diagram(node, model)
    for branch in tree["branches"]:
        label = (f'P{branch["conflict_index"] + 1}: f{branch["firm"]}から'
                 f'w{branch["removed_worker"]}を削除')
        if branch["to"] is None:
            target = f'u{branch["id"]}'
            lines += [f'  {target}["未実行"]:::unexecuted',
                      f'  n{branch["from"]} -.->|"{label}"| {target}']
        else:
            lines.append(f'  n{branch["from"]} ==>|"{label}"| n{branch["to"]}')
    return "\n".join(lines + ["```"])


def render(tree, model, worker_labels=None):
    mode = "Recursive DA（全探索）" if tree["mode"] == "recursive" else "Iterated DA（1回の実行）"
    side = "企業告白型" if tree["proposing_side"] == "firm" else "個人告白型"
    nodes = tree["nodes"]
    lines = [f"# {side} {mode}", "", "[比較と再現方法](README.md)", "",
             "各枠は完全なDAの終了結果。四角が企業、丸が労働者、枠内の線が所属を表す。"
             "線のない点線の労働者は未マッチ。太い矢印は実行した削除、点線矢印は未実行の候補。", "",
             "P番号はその親ノードの違反ペア一覧の順番。異なるペアから同じ削除を選べる場合も枝を分ける。"
             "緑は元の問題でUR安定な終点、赤はUR不安定な終点。"
             "N番号はDAの訪問順で、図の左右の配置は探索順を表さない。", ""]
    if worker_labels is not None:
        lines += ["労働者は削除前の元のラベルで表示する。JSONの添字との対応は入力の `worker_labels_one_based` を参照。", ""]
    if tree["seed"] is not None:
        lines += [f'乱数seed: `{tree["seed"]}`。未実行の枝についてDA結果は計算していない。', ""]
    lines += [mermaid(tree, model), "", "## DAごとの状態", ""]
    for node in nodes:
        deletions = " → ".join(f'(f{d["firm"]},w{d["worker"]})' for d in node["deletions"]) or "なし"
        lines += [f'### N{node["id"]} / DA{node["da_run"]}', "", f"累積削除（順番どおり）: {deletions}。", "",
                  "| 企業 | 所属労働者 |", "|---|---|"]
        lines += [f"| f{firm} | {workers_text(team(node['assignment'], firm))} |" for firm in range(1, model.f + 1)]
        lines += [f"| 未マッチ | {workers_text(team(node['assignment'], 0))} |", ""]
        lines += ["違反ペア: " + (" / ".join(f"P{i + 1}: {pair_text(p)}" for i, p in enumerate(node["conflicts"])) or "なし") + "。", ""]
        if not node["conflicts"]:
            lines += [f'削除後の選好でUR安定: {"はい" if node["reduced_ur_stable"] else "いいえ"}。'
                      f' 元の選好でUR安定: {"はい" if node["original_ur_stable"] else "いいえ"}。', "",
                      "元の問題でのUR逸脱ペア: " + blockers_text(node["original_blocking_pairs"]) + "。", ""]
    lines += ["## 各終点までの削除と移動", "",
              "「残した相方」は削除対象に選ばなかった労働者。以降のDAで所属が維持されるとは限らない。"
              "移動だけで不安定とは判定せず、削除した組が終点でUR逸脱ペアになるかを元の選好で別途調べる。", ""]
    for leaf in tree["leaves"]:
        final = nodes[leaf]
        lines += [f"### 終点 N{leaf}", "",
                  "| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |",
                  "|---|---|---|---|---|---|---|"]
        for step, branch in enumerate(branch_history(tree, leaf), 1):
            pair = nodes[branch["from"]]["conflicts"][branch["conflict_index"]]
            removed, retained = branch["removed_worker"], branch["retained_worker"]
            removed_moves = " → ".join(employer(label) for label in
                (branch["removed_before"], branch["removed_after"], final["assignment"][removed - 1]))
            retained_moves = " → ".join(employer(label) for label in
                (branch["retained_before"], branch["retained_after"], final["assignment"][retained - 1]))
            blocks = {"firm": branch["firm"], "worker": removed} in final["original_blocking_pairs"]
            lines.append(f'| {step} | {pair_text(pair)} | f{branch["firm"]}からw{removed} | w{retained} | '
                         f'{removed_moves} | {retained_moves} | {"はい" if blocks else "いいえ"} |')
        if not branch_history(tree, leaf):
            lines.append("| — | 削除なし | — | — | — | — | — |")
        lines.append("")
    report = "\n".join(lines)
    if worker_labels is not None:
        report = re.sub(r"w(\d+)", lambda match: f"w{worker_labels[int(match[1]) - 1]}", report)
    return report


def main(directory):
    problem = json.loads((directory / "problem.json").read_text())
    worker_labels = problem.get("worker_labels_one_based")
    if worker_labels is not None:
        require(len(worker_labels) == problem["n_workers"]
                and all(type(label) is int and label > 0 for label in worker_labels)
                and len(set(worker_labels)) == len(worker_labels), "invalid worker labels")
    trees = [json.loads((directory / f"{mode}_{side}.json").read_text())
             for mode in ("iterated", "recursive") for side in ("firm", "doctor")]
    reports, checks = {}, []
    for tree in trees:
        model = FirmModel(problem) if tree["proposing_side"] == "firm" else Model(problem)
        checks.append(verify(tree, model))
        reports[tree["algorithm"]] = render(tree, model, worker_labels)
    guide = Path(os.path.relpath(Path(__file__).resolve().parent / "tree_docs" / "README.md", directory)).as_posix()
    lines = ["# DAの削除分岐木", "", f"[実装仕様・実行方法]({guide})", "",
             "UR安定性は削除前の元の選好に対して判定し、空席への追加逸脱を含む。"
             "Recursive DAは全ての違反ペア・両削除対象を探索する。禁止ペアがなくなった枝は終了するが、"
             "他の枝は探索を続け、重複する履歴も残す。", "",
             "| 実行 | DAノード | 終点 | 元の問題でUR安定な終点 | 図と移動履歴 |", "|---|---:|---:|---:|---|"]
    for check in checks:
        name = check["algorithm"]
        lines.append(f'| {name} | {check["nodes"]} | {check["leaves"]} | {check["original_ur_stable_leaves"]} | [{name}]({name}.md) |')
    lines += ["", "Pythonの独立DAモデルで全ノードの割当・違反ペア・累積削除・移動・元/削除後のUR判定を照合した。"
              "Recursive DAの全終点と削除順序は、独立した全経路列挙とも一致した。", "",
              "[照合結果](verification.json) / [入力選好](problem.json)", ""]
    # Validate all four trees before writing any reports.
    for name, report in reports.items():
        (directory / f"{name}.md").write_text(report, encoding="utf-8")
    (directory / "README.md").write_text("\n".join(lines), encoding="utf-8")
    (directory / "verification.json").write_text(json.dumps(checks, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(checks, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(Path(sys.argv[1]).resolve())
