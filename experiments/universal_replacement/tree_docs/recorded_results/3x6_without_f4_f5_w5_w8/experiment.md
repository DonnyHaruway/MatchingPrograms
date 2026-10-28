# さらにw5を除いた3×6実験

実行日: 2026-10-21。

[前回の3×7問題](../3x7_without_f4_f5_w8/problem.json)から労働者w5を取り除いた。最初の5×8問題からはf4・f5・w5・w8を除いた条件となる。企業はf1〜f3、労働者はw1・w2・w3・w4・w6・w7で、定員は `[2,2,2]`。残る選好スコア・相対順位を維持し、w5を含む同僚禁止ペアを除いた。残る禁止ペアは `(w2,w3), (w2,w6)`。

Markdownは元の労働者ラベルを維持する。JSONは計算用の連続した番号1〜6を使い、`problem.json` の `worker_labels_one_based: [1,2,3,4,6,7]` が元のラベルとの対応を示す。例えばJSONのworker=5は元のw6を表す。ランキングの要素・割当配列・違反ペア・削除履歴も同じ計算用番号を使う。

この資料の「元の問題でUR安定」は、再帰的な削除を行う前の **3×6問題** に対する判定を意味する。

## 結果

[個人告白型Recursive DAの分岐図・各状態・移動履歴](recursive_doctor.md) / [4手法の比較](README.md) / [独立検算結果](verification.json) / [入力](problem.json)

| 手法・条件 | DAノード | 終点 | 元の選好でUR安定な終点 |
|---|---:|---:|---:|
| 前回3×7・個人告白型Recursive DA | 7 | 4 | 0 |
| 今回3×6・個人告白型Recursive DA | 3 | 2 | 1 |
| 今回3×6・企業告白型Recursive DA | 3 | 2 | 2 |

個人告白型の初回DAは `f1={w1,w7}, f2={w2,w6}, f3={w3,w4}`。違反ペアはf2の `(w2,w6)` の1組で、両端のどちらを削除しても次のDAで終点となる。

| 削除 | f1 | f2 | f3 | 元の選好でUR安定 | UR逸脱ペア |
|---|---|---|---|---|---|
| f2からw2 | {w1,w2} | {w4,w7} | {w3,w6} | いいえ | (f2,w2) |
| f2からw6 | {w1,w6} | {w2,w7} | {w3,w4} | はい | なし |

全4手法についてPythonの独立DAモデルで各ノードの割当・違反ペア・削除履歴・移動・UR判定を照合した。Recursive DAの全削除経路も独立列挙と一致した。入力が前回の選好行列からw5の行・列だけを除いたものであることと、図の各労働者・所属線が元のラベルに対応していることも確認した。

## 再現方法

`MatchingPrograms` を作業ディレクトリとし、新規の出力先を指定する。seedはIterated DAにのみ適用される。

```sh
c++ -std=c++17 -O2 -I include \
  experiments/universal_replacement/da_search_trees.cpp \
  src/Matching.cpp src/Utils.cpp src/MatchingUtils.cpp \
  src/algorithms.cpp src/algorithmsUtils.cpp \
  -o /tmp/da_search_trees_3x6
/tmp/da_search_trees_3x6 --profile 3x6 --seed 42 --output /tmp/da-trees-3x6
python3 experiments/universal_replacement/render_da_search_trees.py /tmp/da-trees-3x6
```

既存のCMake構成でも `da_search_trees` ターゲットの `--profile 3x6` として実行できる。[共通の実行仕様](../../README.md)を参照。
