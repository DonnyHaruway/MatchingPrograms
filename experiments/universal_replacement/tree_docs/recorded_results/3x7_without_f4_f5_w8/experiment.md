# f4・f5・w8を除いた3×7実験

実行日: 2026-10-21。

[前回の4×7問題](../4x7_without_f5_w8/problem.json)からさらに企業f4を取り除いた。最初の5×8問題からはf4・f5・w8を除いた条件となる。労働者w1〜w7、企業f1〜f3のラベルと残る選好スコア・相対順位は維持した。定員は `[2,2,2]`。同僚選好と禁止ペアは前回から変更していない。

この資料の「元の問題でUR安定」は、再帰的な削除を行う前の **3×7問題** に対する判定を意味する。

## 結果

[個人告白型Recursive DAの分岐図・各状態・移動履歴](recursive_doctor.md) / [4手法の比較](README.md) / [独立検算結果](verification.json) / [3×7の入力](problem.json)

| 手法 | DAノード | 実行した枝 | 終点 | 3×7の元の選好でUR安定な終点 |
|---|---:|---:|---:|---:|
| 個人告白型Recursive DA | 7 | 6 | 4 | 0 |
| 企業告白型Recursive DA | 5 | 4 | 3 | 3 |

全4手法について、前回4×7の全ノードでf4への所属を未マッチに置き換え、枝の移動記録も同様に置き換えると、今回の木のJSONと完全一致した。分岐・残る企業の所属・違反ペア・UR判定は変わらなかった。

Pythonの独立DAモデルで全ノードの割当・違反ペア・削除履歴・移動・UR判定を照合した。Recursive DAの全削除経路も独立列挙と一致した。

## 再現方法

`MatchingPrograms` を作業ディレクトリとし、新規の出力先を指定する。`--profile 3x7` が今回の入力。seedはIterated DAにのみ適用される。

```sh
c++ -std=c++17 -O2 -I include \
  experiments/universal_replacement/da_search_trees.cpp \
  src/Matching.cpp src/Utils.cpp src/MatchingUtils.cpp \
  src/algorithms.cpp src/algorithmsUtils.cpp \
  -o /tmp/da_search_trees_3x7
/tmp/da_search_trees_3x7 --profile 3x7 --seed 42 --output /tmp/da-trees-3x7
python3 experiments/universal_replacement/render_da_search_trees.py /tmp/da-trees-3x7
```

既存のCMake構成でも `da_search_trees` ターゲットの `--profile 3x7` として実行できる。[共通の実行仕様](../../README.md)を参照。
