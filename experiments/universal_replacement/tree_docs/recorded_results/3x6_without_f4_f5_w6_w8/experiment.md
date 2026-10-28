# 3×7問題からw6を除いた実験

実行日: 2026-10-21。

[元の3×7問題](../3x7_without_f4_f5_w8/problem.json)からw6だけを除いた独立ケース。w5は残している。最初の5×8問題からはf4・f5・w6・w8を除いた条件となる。企業はf1〜f3、定員は `[2,2,2]`。残る選好スコア・相対順位・同僚選好は維持し、除いた労働者を含む禁止ペアも取り除いた。

この資料の「元の問題でUR安定」は、再帰的な削除を行う前の、この3×6問題に対する判定を意味する。

Markdownは元の労働者ラベルを使う。JSONは連続した計算用番号1〜6を使い、入力の `worker_labels_one_based: [1, 2, 3, 4, 5, 7]` が元のラベルへの対応を示す。ランキング要素・割当配列・違反ペア・削除履歴も計算用番号を使う。

[個人告白型の分岐図・状態](recursive_doctor.md) / [企業告白型の全分岐](recursive_firm.md) / [4手法の比較](README.md) / [入力](problem.json)

## 結果

| 手法 | DAノード | 終点 | 元の選好でUR安定な終点 |
|---|---:|---:|---:|
| iterated_firm | 2 | 1 | 0 |
| iterated_doctor | 1 | 1 | 1 |
| recursive_firm | 3 | 2 | 1 |
| recursive_doctor | 1 | 1 | 1 |

個人告白型は初回DAで違反ペアがなく、削除を行わずにUR安定な終点に到達した。全員マッチしている。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w1, w5} |
| f2 | {w2, w7} |
| f3 | {w3, w4} |

全4手法の各ノード・割当・違反ペア・削除履歴・移動・UR判定をPythonの独立DAモデルと照合し、Recursive DAの全経路も独立列挙と一致した。入力が3×7問題の対応する行・列を除いたものと完全一致することと、Markdownの所属線が元の労働者ラベルと対応することも確認した。

[独立DA検算](verification.json) / [入力・ラベルの照合](input_verification.json) / [w4・w5・w6削除の比較](../../worker_deletion_comparison.md)

## 再現方法

`MatchingPrograms` を作業ディレクトリとし、新規の出力先を指定する。seedはIterated DAだけに適用される。

```sh
c++ -std=c++17 -O2 -I include \
  experiments/universal_replacement/da_search_trees.cpp \
  src/Matching.cpp src/Utils.cpp src/MatchingUtils.cpp \
  src/algorithms.cpp src/algorithmsUtils.cpp \
  -o /tmp/da_search_trees_worker_cases
/tmp/da_search_trees_worker_cases --profile 3x6-without-w6 --seed 42 --output /tmp/da-trees-without-w6
python3 experiments/universal_replacement/render_da_search_trees.py /tmp/da-trees-without-w6
```
