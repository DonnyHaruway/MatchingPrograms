"""Independently verify every RDA batch profile, aggregate results, and write reports (no figures)."""

import argparse
import csv
import hashlib
import json
import sys
from collections import Counter, defaultdict
from functools import lru_cache
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "iterated_docs"))
from render_iterated_doctor_paths import Model, require, team, workers_text
from render_iterated_firm_paths import FirmModel

CATEGORIES = ("all_stable", "mixed", "none_stable")
DOMAINS = {"complete": "完全選好", "incomplete": "不完全選好"}
SIDES = {"firm": "企業告白型", "doctor": "個人告白型"}


def validate_problem(record, config):
    problem = record["problem"]
    require(record["seed"] == config["seed"], "profile seed mismatch")
    require(problem["n_firms"] == 5 and problem["n_workers"] == 8, "market size")
    require(problem["capacities"] == [2, 2, 2, 2, 1], "capacities")
    for rankings_key, scores_key, alternatives, lengths in (
        ("worker_rankings_one_based", "agent_prefs", 5, config["worker_list_lengths"]),
        ("firm_rankings_one_based", "firm_prefs", 8, config["firm_list_lengths"]),
    ):
        for i, order in enumerate(problem[rankings_key]):
            expected_length = alternatives if record["domain"] == "complete" else lengths[i]
            require(len(order) == len(set(order)) == expected_length, "acceptable list length or ties")
            require(all(1 <= label <= alternatives for label in order), "ranking label")
            scores = [-100000] * alternatives
            for rank, label in enumerate(order):
                scores[label - 1] = len(order) - rank
            require(scores == problem[scores_key][i], "ranking scores")
    cp = problem["colleague_prefs"]
    require(len(cp) == 8 and all(len(row) == 8 for row in cp), "colleague dimensions")
    pairs = []
    for left in range(8):
        require(cp[left][left] == 0, "diagonal rejection")
        for right in range(left + 1, 8):
            require(cp[left][right] == cp[right][left] and cp[left][right] in (0, -100000), "binary symmetric rejection")
            if cp[left][right] == -100000:
                pairs.append([left + 1, right + 1])
    require(pairs == problem["forbidden_pairs"] and len(pairs) == 9, "exactly nine unordered rejection pairs")


def independent_search(problem, firm):
    model = FirmModel(problem) if firm else Model(problem)

    @lru_cache(maxsize=None)
    def visit(deleted):
        step = model.full_da(deleted)
        conflicts = step["conflicts"]
        if not conflicts:
            assignment = tuple(step["assignment"])
            stable = not model.blockers(assignment)
            return {"nodes": 1, "leaves": 1, "stable_leaves": int(stable),
                    "finals": frozenset([assignment]), "stable_finals": frozenset([assignment]) if stable else frozenset(),
                    "max_conflicts": 0, "max_deletions": len(deleted)}
        nodes, leaves, stable_leaves = 1, 0, 0
        finals, stable_finals = set(), set()
        max_conflicts, max_deletions = len(conflicts), len(deleted)
        for pair in conflicts:
            for removed in (pair["left"], pair["right"]):
                edge = (pair["firm"], removed)
                require(edge not in deleted, "repeated deletion")
                child = visit(deleted | {edge})
                # Add the subtree for EACH branch, even if cached: duplicates retain their multiplicity.
                nodes += child["nodes"]
                leaves += child["leaves"]
                stable_leaves += child["stable_leaves"]
                finals.update(child["finals"])
                stable_finals.update(child["stable_finals"])
                max_conflicts = max(max_conflicts, child["max_conflicts"])
                max_deletions = max(max_deletions, child["max_deletions"])
        return {"nodes": nodes, "leaves": leaves, "stable_leaves": stable_leaves,
                "finals": frozenset(finals), "stable_finals": frozenset(stable_finals),
                "max_conflicts": max_conflicts, "max_deletions": max_deletions}

    result = visit(frozenset())
    expected = {key: result[key] for key in ("nodes", "leaves", "stable_leaves", "max_conflicts", "max_deletions")}
    expected.update(branches=result["nodes"] - 1, unstable_leaves=result["leaves"] - result["stable_leaves"],
                    distinct_matchings=len(result["finals"]), distinct_stable_matchings=len(result["stable_finals"]),
                    initial_conflicts=len(model.full_da(frozenset())["conflicts"]),
                    min_matched_workers=min(sum(label != 0 for label in a) for a in result["finals"]),
                    max_matched_workers=max(sum(label != 0 for label in a) for a in result["finals"]))
    expected["category"] = "all_stable" if result["stable_leaves"] == result["leaves"] else "mixed" if result["stable_leaves"] else "none_stable"
    return model, visit, expected


def verify_witness(witness, stable, model, visit):
    require(witness is not None, "missing witness")
    deleted, node_id = frozenset(), 0
    for step in witness["steps"]:
        before = model.full_da(deleted)
        require(step["assignment_before"] == before["assignment"], "witness parent DA")
        pair = before["conflicts"][step["conflict_index"]]
        require(all(step[key] == pair[key] for key in ("firm", "left", "right")), "witness selected conflict")
        require(step["removed_worker"] in (pair["left"], pair["right"]), "witness deletion endpoint")
        retained = pair["right"] if step["removed_worker"] == pair["left"] else pair["left"]
        require(step["retained_worker"] == retained, "witness retained endpoint")
        next_id = node_id + 1
        found = False
        for index, alternative in enumerate(before["conflicts"]):
            for endpoint in (alternative["left"], alternative["right"]):
                if index == step["conflict_index"] and endpoint == step["removed_worker"]:
                    found = True
                    break
                next_id += visit(deleted | {(alternative["firm"], endpoint)})["nodes"]
            if found:
                break
        require(found and step["parent_node"] == node_id and step["child_node"] == next_id, "DFS witness node IDs")
        node_id = next_id
        deleted = deleted | {(pair["firm"], step["removed_worker"])}
        after = model.full_da(deleted)
        require(step["assignment_after"] == after["assignment"], "witness child DA")
        require(step["retained_after"] == after["assignment"][retained - 1], "retained immediate assignment")
        require(step["retained_final"] == witness["assignment"][retained - 1], "retained final assignment")
    final = model.full_da(deleted)
    require(not final["conflicts"] and final["assignment"] == witness["assignment"], "witness terminal DA")
    blockers = model.blockers(witness["assignment"])
    require(witness["original_blocking_pairs"] == blockers, "witness original UR blockers")
    require(witness["original_ur_stable"] == stable == (not blockers), "witness stability")
    require(witness["leaf_id"] == node_id and witness["reduced_ur_stable"], "witness leaf / reduced stability")
    require(all((p["firm"], p["worker"]) in deleted for p in blockers), "blocking pair was not deleted")


def without_times(record):
    clone = json.loads(json.dumps(record))
    for outcome in clone["algorithms"].values():
        outcome.pop("algorithm_seconds")
    return clone


def csv_file(path, rows):
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def rate(count, total):
    return f"{count} ({100 * count / total:.1f}%)" if total else "0 (—)"


def case_report(record, side):
    problem = record["problem"]
    outcome = record["algorithms"][side]
    witness = outcome["unstable_witness"]
    employer = lambda label: f"f{label}" if label else "未マッチ"
    model = FirmModel(problem) if side == "firm" else Model(problem)
    lines = [f'## {DOMAINS[record["domain"]]}・trial {record["trial"]}・{SIDES[side]}', "",
             f'分類: `{outcome["category"]}`。全{outcome["leaves"]}経路のうち、'
             f'UR安定{outcome["stable_leaves"]}、UR不安定{outcome["unstable_leaves"]}。', "",
             "### 入力選好", "", "| 企業 | 順位 |", "|---|---|"]
    for i, order in enumerate(problem["firm_rankings_one_based"], 1):
        lines.append(f'| f{i} | ' + " > ".join(f"w{w}" for w in order) + " |")
    lines += ["", "| 労働者 | 順位 |", "|---|---|"]
    for i, order in enumerate(problem["worker_rankings_one_based"], 1):
        lines.append(f'| w{i} | ' + " > ".join(f"f{f}" for f in order) + " |")
    lines += ["", "禁止ペア: " + "、".join(f"(w{a},w{b})" for a, b in problem["forbidden_pairs"]) + "。", "",
              "### 不安定な経路", "", "| 順番 | 違反ペア | 削除 | 相方の所属：親→直後→終点 |", "|---|---|---|---|"]
    for index, step in enumerate(witness["steps"], 1):
        moves = " → ".join(employer(label) for label in (step["firm"], step["retained_after"], step["retained_final"]))
        lines.append(f'| {index} | f{step["firm"]}: (w{step["left"]},w{step["right"]}) | '
                     f'f{step["firm"]}からw{step["removed_worker"]} | w{step["retained_worker"]}: {moves} |')
    lines += ["", "| 終点の企業 | 所属労働者 |", "|---|---|"]
    lines += [f'| f{firm} | {workers_text(team(witness["assignment"], firm))} |' for firm in range(1, 6)]
    lines += [f'| 未マッチ | {workers_text(team(witness["assignment"], 0))} |', "",
              "削除後の選好ではUR安定。元の選好では以下のUR逸脱ペアがある。", ""]
    for blocker in witness["original_blocking_pairs"]:
        firm, worker = blocker["firm"], blocker["worker"]
        members = team(witness["assignment"], firm)
        current = witness["assignment"][worker - 1]
        lines.append(f'- **(f{firm},w{worker})**: w{worker}は{employer(current)}よりf{firm}を厳密に好む。')
        if len(members) < problem["capacities"][firm - 1]:
            lines.append(f'  f{firm}に空席があり、追加は企業の評価を改善し、追加後の同僚制約も満たす。')
        else:
            inferior = [w for w in members if problem["firm_prefs"][firm - 1][worker - 1] > problem["firm_prefs"][firm - 1][w - 1]]
            lines.append(f'  f{firm}はw{worker}を在籍者{workers_text(inferior)}より厳密に好み、どの在籍者を置き換えても同僚制約を満たす。')
            for removed in members:
                after = sorted([w for w in members if w != removed] + [worker])
                require(model.compatible(after), "reported counterexample replacement")
                lines.append(f'  w{removed}の置換後: {workers_text(after)}。')
        lines.append("")
    return lines


def main(directory, pilot):
    config = json.loads((directory / "config.json").read_text())
    completion = json.loads((directory / "completion.json").read_text())
    require(completion["complete"] and completion["profiles_per_domain"] == config["trials_per_domain"], "incomplete experiment")
    records = [json.loads(line) for line in (directory / "instances.jsonl").read_text().splitlines()]
    require(len(records) == completion["records"] == 2 * config["trials_per_domain"], "record count")
    expected_keys = {(domain, trial) for domain in DOMAINS
                     for trial in range(config["start_trial"], config["start_trial"] + config["trials_per_domain"])}
    indexed = {(r["domain"], r["trial"]): r for r in records}
    require(set(indexed) == expected_keys and len(indexed) == len(records), "profile identities")
    totals, strata, joint = defaultdict(Counter), defaultdict(Counter), Counter()
    rows, representatives = [], {}
    verified_witnesses, verified_algorithms, cached_states = 0, 0, 0
    for index, record in enumerate(records, 1):
        validate_problem(record, config)
        domain, trial = record["domain"], record["trial"]
        for side in SIDES:
            outcome = record["algorithms"][side]
            model, visit, expected = independent_search(record["problem"], side == "firm")
            for key, value in expected.items():
                require(outcome[key] == value, f"{domain}/{trial}/{side}: {key}: {outcome[key]} != {value}")
            for stable, key, count in ((True, "stable_witness", outcome["stable_leaves"]),
                                       (False, "unstable_witness", outcome["unstable_leaves"])):
                if count:
                    verify_witness(outcome[key], stable, model, visit)
                    verified_witnesses += 1
                else:
                    require(outcome[key] is None, "unexpected witness")
            verified_algorithms += 1
            cached_states += visit.cache_info().currsize
            group = totals[domain, side]
            group[outcome["category"]] += 1
            group["profiles"] += 1
            group["nodes"] += outcome["nodes"]
            group["leaves"] += outcome["leaves"]
            group["max_nodes"] = max(group["max_nodes"], outcome["nodes"])
            group["max_deletions"] = max(group["max_deletions"], outcome["max_deletions"])
            if outcome["initial_conflicts"]:
                group["nontrivial"] += 1
                group["nontrivial_" + outcome["category"]] += 1
            strata[domain, side, outcome["initial_conflicts"]][outcome["category"]] += 1
            rows.append(dict(domain=domain, trial=trial, proposing_side=side,
                             **{key: value for key, value in outcome.items() if not key.endswith("witness")}))
            rep_key = f'{domain}/{side}/{outcome["category"]}'
            previous = representatives.get(rep_key)
            if previous is None or (outcome["nodes"], trial) < (previous["algorithms"][side]["nodes"], previous["trial"]):
                representatives[rep_key] = record
            visit.cache_clear()
        joint[domain, record["algorithms"]["firm"]["category"], record["algorithms"]["doctor"]["category"]] += 1
        if index % 100 == 0 or index == len(records):
            print(f"Independently verified profiles: {index}/{len(records)}", flush=True)
    for trial in range(config["start_trial"], config["start_trial"] + config["trials_per_domain"]):
        complete, incomplete = (indexed[d, trial]["problem"] for d in DOMAINS)
        require(complete["colleague_prefs"] == incomplete["colleague_prefs"], "paired rejection graphs")
        for key, lengths in (("worker_rankings_one_based", config["worker_list_lengths"]),
                             ("firm_rankings_one_based", config["firm_list_lengths"])):
            require(all(short == full[:length] for short, full, length in zip(incomplete[key], complete[key], lengths)), "paired rankings")
    require(sum(group["nodes"] for group in totals.values()) == completion["total_nodes"], "total visited nodes")
    require(max(group["max_nodes"] for group in totals.values()) == completion["largest_tree"], "largest tree")
    pilot_prefix = None
    if pilot:
        pilot_records = [json.loads(line) for line in (pilot / "instances.jsonl").read_text().splitlines()]
        require(all(without_times(r) == without_times(indexed[r["domain"], r["trial"]]) for r in pilot_records), "pilot prefix mismatch")
        pilot_prefix = len(pilot_records)
    summary = []
    for (domain, side), group in sorted(totals.items()):
        summary.append(dict(domain=domain, proposing_side=side, profiles=group["profiles"],
                            **{key: group[key] for key in CATEGORIES}, initial_conflicts_positive=group["nontrivial"],
                            **{"with_initial_conflicts_" + key: group["nontrivial_" + key] for key in CATEGORIES},
                            total_nodes=group["nodes"], total_leaves=group["leaves"], max_nodes=group["max_nodes"],
                            max_deletions=group["max_deletions"]))
    csv_file(directory / "summary.csv", summary)
    csv_file(directory / "profiles.csv", rows)
    csv_file(directory / "by_initial_conflicts.csv", [dict(domain=d, proposing_side=s, initial_conflicts=k,
             profiles=sum(counts.values()), **{c: counts[c] for c in CATEGORIES}) for (d, s, k), counts in sorted(strata.items())])
    csv_file(directory / "joint_categories.csv", [dict(domain=d, firm_category=f, doctor_category=w, profiles=n)
             for (d, f, w), n in sorted(joint.items())])
    with (directory / "firm_counterexamples.jsonl").open("w", encoding="utf-8") as stream:
        for record in records:
            if record["algorithms"]["firm"]["unstable_leaves"]:
                stream.write(json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n")
    (directory / "representative_cases.json").write_text(json.dumps(representatives, ensure_ascii=False, indent=2) + "\n")
    lines = ["# RDAの複数ケース実験", "", "企業5・労働者8、定員(2,2,2,2,1)、相互拒否ペア9組、同順位なし。"
             "全ケースで両提案側の削除経路を最後まで探索し、元の選好でUR判定した。空席への追加逸脱を含む。", "",
             f'基準seed: `{config["seed"]}`。各条件{config["trials_per_domain"]:,}件。'
             f'試行番号は{config["start_trial"]}〜{config["start_trial"] + config["trials_per_domain"] - 1}。', "",
             "## 問題ごとの分類", "", "| 選好 | 提案側 | 問題数 | 全経路安定 | 混在 | 全経路不安定 |", "|---|---|---:|---:|---:|---:|"]
    for row in summary:
        lines.append(f'| {DOMAINS[row["domain"]]} | {SIDES[row["proposing_side"]]} | {row["profiles"]} | '
                     + " | ".join(rate(row[c], row["profiles"]) for c in CATEGORIES) + " |")
    lines += ["", "全経路安定＝全終点がUR安定。混在＝安定・不安定の両方の終点が存在。"
              "全経路不安定＝RDAが到達する終点にUR安定なものがない。", "",
              "## 最初のDAで違反があった問題だけの分類", "",
              "| 選好 | 提案側 | 対象問題数 | 全経路安定 | 混在 | 全経路不安定 |", "|---|---|---:|---:|---:|---:|"]
    for row in summary:
        n = row["initial_conflicts_positive"]
        lines.append(f'| {DOMAINS[row["domain"]]} | {SIDES[row["proposing_side"]]} | {n} | '
                     + " | ".join(rate(row["with_initial_conflicts_" + c], n) for c in CATEGORIES) + " |")
    firm_failures = sum(r["mixed"] + r["none_stable"] for r in summary if r["proposing_side"] == "firm")
    lines += ["", "## 今回確認できたこと", ""]
    if firm_failures:
        lines.append(f'企業告白型でも、不安定な削除経路を含む問題が計{firm_failures}件あった。'
                     "したがって、元の5×8サンプルで見られた「企業告白型ならどの経路でもUR安定」という性質は、今回の入力範囲全体には成り立たない。")
    else:
        lines.append("企業告白型で不安定な削除経路は今回の標本では見つからなかった。これは全選好に対する保証ではない。")
    if all(r["none_stable"] == 0 for r in summary):
        lines += ["", "一方、今回は両提案側・両選好条件の全問題で少なくとも1本のUR安定な経路があった。"
                  "元の固定サンプルの個人告白型では全4経路が不安定だったため、この存在の傾向も一般保証ではない。"]
    lines += ["", "## 検算と再現", "",
              f'全{verified_algorithms:,}探索の経路数・安定終点数・異なる終点数・深さ等を独立Python DAモデルで照合し、'
              f'保存した{verified_witnesses:,}本の証拠経路の全DA・削除・UR逸脱ペアも再計算した。', "",
              "検算側では同じ削除集合の計算を再利用するが、各枝の経路数を足し戻すため、重複経路は保持される。"
              "C++のRDA本体は枝刈り・状態の統合を行っていない。", "",
              f'探索ノード総数: {completion["total_nodes"]:,}。最大の木: {completion["largest_tree"]:,}ノード。'
              f'C++実験実行時間: {completion["elapsed_seconds"]:.3f}秒（検算・ビルド時間を含まない）。', ""]
    if pilot_prefix is not None:
        lines += [f'予備実験の{pilot_prefix}問題は本実験の先頭部分と、測定時間を除く入力・出力が一致した。'
                  "本集計では予備実験を重複加算していない。", ""]
    lines += ["[集計CSV](summary.csv) / [問題別CSV](profiles.csv) / [初期違反数別](by_initial_conflicts.csv) / "
              "[同一問題での提案側比較](joint_categories.csv)", "",
              "[企業側の反例の具体例](counterexamples.md) / [企業側の反例全件](firm_counterexamples.jsonl) / "
              "[全入力と証拠経路](instances.jsonl) / [検算結果](verification.json) / [実験条件](config.json)", "",
              "## 適用範囲", "", "これはランダムに生成した入力についての結果で、全選好の列挙や一般定理の証明ではない。"
              "完全選好と不完全選好は同じ順位の基礎順列と禁止ペアを共有し、不完全選好では各リストをサンプルと同じ長さに切り詰める。"
              "各条件の周辺分布は、指定長の順序付きリストの一様抽出となる。", "",
              "経路数は重複した削除履歴を含むため、安定な経路の割合はIterated DAの乱択成功確率を表さない。"
              "RDAの到達先だけを調べており、全経路不安定でも市場にUR安定マッチングが存在しないとは限らない。"
              "各問題の図は生成していない。", ""]
    (directory / "report.md").write_text("\n".join(lines), encoding="utf-8")
    counterexamples = ["# 企業告白型RDAの反例", "", "[全体の集計](report.md)", "",
                       "各選好条件から探索ノード数が最小の反例を選び、同数なら試行番号が小さいものを掲載する。"
                       "図は生成せず、元の入力と1本の不安定な経路を示す。", ""]
    for domain in DOMAINS:
        candidates = [representatives[key] for key in representatives if key.startswith(domain + "/firm/") and not key.endswith("all_stable")]
        if candidates:
            selected = min(candidates, key=lambda r: (r["algorithms"]["firm"]["nodes"], r["trial"]))
            counterexamples += case_report(selected, "firm")
    replacement_found = False
    for domain in DOMAINS:
        candidates = []
        for record in records:
            if record["domain"] != domain:
                continue
            witness = record["algorithms"]["firm"]["unstable_witness"]
            if witness and any(witness["assignment"].count(pair["firm"]) == record["problem"]["capacities"][pair["firm"] - 1]
                               for pair in witness["original_blocking_pairs"]):
                candidates.append(record)
        if candidates:
            replacement_found = True
            selected = min(candidates, key=lambda r: (r["algorithms"]["firm"]["nodes"], r["trial"]))
            counterexamples += ["## 満員企業への置換逸脱がある反例", "",
                               "次の反例は空席への追加逸脱を除外しても不安定となる。"
                               "保存した証拠経路に満員企業のUR逸脱を含むケースから、探索ノード数が最小のものを選んだ。", ""]
            counterexamples += case_report(selected, "firm")
    if replacement_found:
        # Keep this finding near the main conclusion as well as in the detailed evidence.
        marker = "## 検算と再現"
        report = (directory / "report.md").read_text()
        report = report.replace(marker, "企業告白型の反例には、空席への追加逸脱だけでなく、満員企業への置換によるUR逸脱もある。"
                                "具体的な選好と全置換の確認を[反例資料](counterexamples.md)に示した。\n\n" + marker)
        (directory / "report.md").write_text(report, encoding="utf-8")
    (directory / "counterexamples.md").write_text("\n".join(counterexamples), encoding="utf-8")
    verification = dict(profiles=len(records), algorithms=verified_algorithms, witnesses=verified_witnesses,
                        all_inputs_valid=True, all_search_counts_independently_verified=True,
                        all_witnesses_independently_verified=True, paired_domains_verified=True,
                        pilot_prefix_profiles_verified=pilot_prefix, cached_independent_states=cached_states,
                        cpp_nodes_including_duplicate_histories=completion["total_nodes"], figures_generated=False)
    (directory / "verification.json").write_text(json.dumps(verification, indent=2) + "\n")
    source_root = Path(__file__).resolve().parents[2]
    sources = ["src/algorithms.cpp", "src/Matching.cpp", "src/Utils.cpp", "src/algorithmsUtils.cpp",
               "include/algorithms.hpp", "include/Matching.hpp", "include/algorithmsUtils.hpp", "include/CommonTypes.hpp",
               "experiments/universal_replacement/problem_5x8.hpp",
               "experiments/universal_replacement/rda_batch.hpp", "experiments/universal_replacement/run_rda_batch.cpp",
               "experiments/universal_replacement/analyze_rda_batch.py",
               "experiments/universal_replacement/iterated_docs/render_iterated_doctor_paths.py",
               "experiments/universal_replacement/iterated_docs/render_iterated_firm_paths.py"]
    hashes = {name: hashlib.sha256((source_root / name).read_bytes()).hexdigest() for name in sources}
    (directory / "source_sha256.json").write_text(json.dumps(hashes, indent=2) + "\n")
    print(json.dumps(verification, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--pilot", type=Path)
    args = parser.parse_args()
    main(args.directory.resolve(), args.pilot.resolve() if args.pilot else None)
