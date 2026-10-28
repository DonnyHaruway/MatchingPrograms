# Iterated DAの実行履歴とRecursive DAの全探索

[5企業・8労働者の比較と図](recorded_results/5x8/README.md) / [提示された2企業・4労働者の例](recorded_results/example/README.md)

[難しい例1（3企業・5人）の入力・全終点・再現方法](recorded_results/difficult_example_1_3x5/experiment.md) / [企業告白型RDAの全分岐](recorded_results/difficult_example_1_3x5/recursive_firm.md) / [個人告白型RDAの全分岐](recorded_results/difficult_example_1_3x5/recursive_doctor.md)

[f5・w8を除いた4企業・7労働者の実験](recorded_results/4x7_without_f5_w8/experiment.md) / [個人告白型の全分岐](recorded_results/4x7_without_f5_w8/recursive_doctor.md)

[さらにf4を除いた3企業・7労働者の実験](recorded_results/3x7_without_f4_f5_w8/experiment.md) / [個人告白型の全分岐](recorded_results/3x7_without_f4_f5_w8/recursive_doctor.md)

[さらにw5を除いた3企業・6労働者の実験](recorded_results/3x6_without_f4_f5_w5_w8/experiment.md) / [個人告白型の全分岐](recorded_results/3x6_without_f4_f5_w5_w8/recursive_doctor.md)

[3×7問題からw4・w5・w6をそれぞれ除いた比較](worker_deletion_comparison.md) / [w4削除の実験](recorded_results/3x6_without_f4_f5_w4_w8/experiment.md) / [w6削除の実験](recorded_results/3x6_without_f4_f5_w6_w8/experiment.md)

ランダムに生成した複数問題でのRDA全探索は [5×8複数ケース実験](../rda_batch.md) を参照。

| 5×8の全探索 | 終点（削除履歴） | 元の問題でUR安定 |
|---|---:|---:|
| 企業告白型Recursive DA | 3 | 3 |
| 個人告白型Recursive DA | 4 | 0 |

これらはこの固定問題での結果。一般の入力に対する安定性の保証ではない。
既存のIterated DAの全選択経路を列挙した結果とも一致する。

## Recursive DAの仕様

1. 同僚選好を無視し、通常DAを全員未マッチ・提案履歴なしから最後まで実行する。
2. 同じ企業に所属する違反ペアがなければ、その枝の終点として記録する。他の枝の探索は続ける。
3. 全企業の違反ペアそれぞれについて、片方ずつを該当企業の選好から削除する2枝を作る。
4. 各枝で過去の削除を累積した選好を使って再帰する。兄弟の枝に削除を持ち込まない。

違反ペアは、同じ企業の2労働者のどちらか一方向でも同僚選好が `-INF` である組。
枝の順番は企業ID、ペアの小さい労働者ID、大きい労働者IDの順、各ペアで小さい側の削除→大きい側の削除。
各非終端ノードの枝数は厳密に「違反ペア数×2」。同じ労働者の削除に至る枝も統合しない。
枝刈り・状態の統合・安定解発見による打ち切りは行わない。

元の選好、労働者の企業選好、同僚選好、定員は変更しない。削除はその企業との関係だけに適用する。
通常DAの同順位・受け入れ不可の扱いは既存実装をそのまま使用する。

## Iterated DAの図

通常のIterated DAの1回の実行を記録する。乱数seedと選択処理は既存実装と同一。
各DA終了時の全違反ペア・両端点を候補として残し、実際に選んだ枝だけに次のDAノードを接続する。
未選択の枝は点線と「未実行」で示し、仮のマッチング結果は付けない。

図では各DA結果を企業と労働者を線で結ぶマッチングとして描く。
全企業（空の企業を含む）と全労働者（未マッチを含む）を表示する。
各枠のDA番号は完全なDAの実行回数であり、DA内部の提案回数ではない。

## 安定性と移動の記録

「違反ペアなし」を探索上の終了条件とし、元の問題でのUR安定性は別に判定する。
UR判定は現行の `Matching` の定義を使い、満員企業のuniversal replacementと空席への追加逸脱を含む。
同僚選好はbinaryの拒否制約を想定する。

各ノードは元の選好と削除後の選好の両方でUR判定される。
削除後の問題でUR安定でも、削除した企業・労働者の組が元の問題でUR逸脱になる可能性がある。
元の問題で安定と判定する際は、過去に削除した組もすべて候補に戻して検査する。
違反ペアが残る中間ノードは実行可能でないためUR安定とは判定しない。

各枝は選択した違反ペア、削除した労働者、残した相方、両者の親/直後の所属を記録する。
各終点の表には終点での両者の所属も載せ、過去に残した相方が後のDAで移動する場合も追跡できる。
相方の移動だけで不安定とは判断せず、削除した組が実際にUR逸脱ペアになったかを併記する。

## C++からの利用

```cpp
#include "MatchingSystem.hpp"

// systemの選好・定員を設定後。全終点を返すため、単一Matchingのrun_algorithmとは別API。
auto tree = system.run_recursive_algorithm("recursive_firm_propose_DA");
// 個人告白型: "recursive_doctor_propose_DA"
for (auto id : tree.leaves) {
    const auto &leaf = tree.nodes[id];
    const Matching &matching = leaf.matching;
    const bool stable_in_original_problem = leaf.original_ur_stable;
}

// 行列を直接渡す場合。個人告白型はrecursive_doctor_proposing_DA_algorithm。
auto all = recursive_firm_proposing_DA_algorithm(
    n_agents, n_firms, capacities, agent_prefs, firm_prefs, colleague_prefs);

// Iterated DAは既存関数の引数・返り値を保持。履歴が必要ならtrace関数を使う。
auto trace = trace_iterated_firm_proposing_DA_algorithm(
    n_agents, n_firms, capacities, agent_prefs, firm_prefs, colleague_prefs, 42);
// 個人告白型: trace_iterated_doctor_proposing_DA_algorithm
```

`DASearchTree` は `nodes`、`branches`、`leaves` を保持する。
`branches[i].conflict_index` は親ノードの違反ペア一覧のインデックスで、同じ削除でも選んだペアを区別する。
`branches[i].to == std::nullopt` は未実行の候補。`leaves` は安定/不安定にかかわらず違反ペアのない実行済みノード。
C++の企業・労働者・ノード・選択インデックスは0始まり。`original_blocking_pairs` は既存APIと同じ `(worker, firm)`。

## 実行と再生成

`MatchingPrograms` を作業ディレクトリにする。既存のbuildが以前の移動前のパスを参照している場合も動くよう、新しいビルド先を使う。

```sh
cmake -S . -B /tmp/matching-da-trees-build
cmake --build /tmp/matching-da-trees-build --target da_search_trees -j4

/tmp/matching-da-trees-build/experiments/universal_replacement/da_search_trees \
  --profile 5x8 --seed 42 --output /tmp/da-trees-5x8
python3 experiments/universal_replacement/render_da_search_trees.py /tmp/da-trees-5x8

/tmp/matching-da-trees-build/experiments/universal_replacement/da_search_trees \
  --profile example --seed 42 --output /tmp/da-trees-example
python3 experiments/universal_replacement/render_da_search_trees.py /tmp/da-trees-example
```

出力先は新規ディレクトリを指定する。既存ディレクトリには上書きしない。
`--output` 省略時は `experiments/universal_replacement/outputs/da_trees_<時刻>` に作成する。
`--seed` はIterated DAだけに使用する。Recursive DAは決定的な順番で全探索する。
出力には `problem.json` と4種類の木のJSON、独立検算後には対応するMarkdownと `verification.json` が含まれる。
JSONの企業・労働者ラベルは1始まり、未マッチは0。ノードID・枝ID・`conflict_index` は0始まり。

`--profile 3x6` はf4・f5・w5・w8を除いた問題。労働者の元のラベルは `problem.json` の `worker_labels_one_based` に保存する。JSONの計算用番号をこの配列で元のラベルに対応させ、Markdownでは元のラベルを表示する。

`--profile 3x6-without-w4` と `--profile 3x6-without-w6` は、3×7問題からそれぞれw4だけ、w6だけを除いた問題。両ケースともw5は残る。

`--profile difficult-example-1` は提示画像「難しい例1.png」の3企業・5人の問題。定員は `[2,2,1]`、禁止ペアは `{w2,w3}` と `{w2,w5}`。両告白側の全分岐を探索し、5×8と同じ形式で保存する。

保存済みの結果を再検算して図を再生成する場合：

```sh
python3 experiments/universal_replacement/render_da_search_trees.py \
  experiments/universal_replacement/tree_docs/recorded_results/5x8
python3 experiments/universal_replacement/render_da_search_trees.py \
  experiments/universal_replacement/tree_docs/recorded_results/example
```

Pythonは既存の独立DAモデルを利用し、C++を呼ばずに各状態と全削除経路を検算してから資料を生成する。
過去の `iterated_docs` の保存データ・資料は変更しない。

## 計算量

各枝では未削除の企業・労働者の組を1つ削除するため、深さは元の受け入れ可能な組数以下。
ただし全探索の経路数は指数的に増えうる。重複した木も保存するため、大きい入力では時間とメモリが増える。
