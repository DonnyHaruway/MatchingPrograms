# 難しい例1：3企業・5人のRDA実験

実行日: 2026-10-21。提示画像「難しい例1.png」の入力を使用した。

[企業告白型RDAの全分岐図・状態・移動履歴](recursive_firm.md) / [個人告白型RDAの全分岐図・状態・移動履歴](recursive_doctor.md) / [機械生成の比較表](README.md)

## 入力

定員は `(q_f1,q_f2,q_f3) = (2,2,1)`。

| 企業 | 労働者の選好（左ほど上位） |
|---|---|
| f1 | w1 ≻ w5 ≻ w2 ≻ w4 ≻ w3 |
| f2 | w3 ≻ w4 ≻ w2 ≻ w1 ≻ w5 |
| f3 | w1 ≻ w4 ≻ w5 ≻ w3 ≻ w2 |

| 労働者 | 企業の選好（左ほど上位） |
|---|---|
| w1 | f3 ≻ f1 ≻ f2 |
| w2 | f2 ≻ f1 ≻ f3 |
| w3 | f2 ≻ f3 ≻ f1 |
| w4 | f3 ≻ f1 ≻ f2 |
| w5 | f2 ≻ f1 ≻ f3 |

禁止ペアは `B = {{w2,w3},{w2,w5}}`。両方向の同僚選好を `-INF`、その他を0とした。
企業・労働者の選好は順位に沿った正の整数スコアに変換した。全ての企業・労働者の組は、再帰的削除を行う前は受け入れ可能である。

[計算に使用した入力](problem.json) / [画像から転記した条件と全スコア行列の照合](input_verification.json)

## 実行方法と集計

既存のRecursive DA（RDA）を使用した。各ノードで通常DAを最初から実行し、同じ企業に所属する禁止ペアそれぞれについて、片方ずつをその企業の選好から削除する全分岐を探索した。削除は経路上で累積する。禁止ペアがない枝で終了し、他の枝も最後まで探索した。

「元の問題でUR安定」は、削除前の上記入力に対する判定であり、空席への追加逸脱も含む。削除後の選好でのUR安定性は別に記録した。

| 告白側 | DAノード | 削除分岐 | 終点 | 元の問題でUR安定な終点 | 元の問題でUR不安定な終点 |
|---|---:|---:|---:|---:|---:|
| 企業告白型RDA | 11 | 10 | 6 | 2 | 4 |
| 個人告白型RDA | 5 | 4 | 3 | 2 | 1 |

終点数は削除履歴ごとに数える。この例では各方式の終点マッチングにも重複はない。両方式で得られたUR安定なマッチングは同じ2種類だった。

## 企業告白型RDAの全終点

初回DAは `f1={w2,w5}, f2={w3,w4}, f3={w1}`。f1の禁止ペア `{w2,w5}` から分岐する。

削除列の `(f_i,w_j)` は、企業f_iの選好からw_jを削除したことを表す。N番号は企業告白型の図に対応する。

| 終点 | 累積削除（順番どおり） | f1 | f2 | f3 | 未マッチ | 元の問題でUR安定 | 元の問題でのUR逸脱ペア |
|---|---|---|---|---|---|---|---|
| N2 | (f1,w2) → (f2,w2) | {w4} | {w3,w5} | {w1} | {w2} | いいえ | (f1,w2) |
| N4 | (f1,w2) → (f2,w3) → (f2,w2) | {w3,w4} | {w5} | {w1} | {w2} | いいえ | (f2,w3) |
| N5 | (f1,w2) → (f2,w3) → (f2,w5) | {w4,w5} | {w2} | {w1} | {w3} | はい | なし |
| N7 | (f1,w5) → (f2,w2) | {w2,w4} | {w3,w5} | {w1} | なし | はい | なし |
| N9 | (f1,w5) → (f2,w3) → (f2,w2) | {w2,w4} | {w5} | {w1} | {w3} | いいえ | (f2,w3) |
| N10 | (f1,w5) → (f2,w3) → (f2,w5) | {w3,w4} | {w2} | {w1} | {w5} | いいえ | (f1,w5) |

## 個人告白型RDAの全終点

初回DAは `f1={w4,w5}, f2={w2,w3}, f3={w1}`。f2の禁止ペア `{w2,w3}` から分岐する。N番号は個人告白型の図に対応する。

| 終点 | 累積削除（順番どおり） | f1 | f2 | f3 | 未マッチ | 元の問題でUR安定 | 元の問題でのUR逸脱ペア |
|---|---|---|---|---|---|---|---|
| N1 | (f2,w2) | {w2,w4} | {w3,w5} | {w1} | なし | はい | なし |
| N3 | (f2,w3) → (f2,w2) | {w2,w4} | {w5} | {w1} | {w3} | いいえ | (f2,w3) |
| N4 | (f2,w3) → (f2,w5) | {w4,w5} | {w2} | {w1} | {w3} | はい | なし |

両方式とも全終点は削除後の選好ではUR安定だった。一方、元の選好に戻すとUR不安定な終点が残る。禁止ペアがなくなったことだけでは、元の問題でのUR安定性は保証されない。

## 検算と保存ファイル

既存のPython独立DAモデルで、全ノードの割当・禁止ペア・累積削除・移動・元/削除後のUR判定をC++の出力と照合した。RDAの全終点と削除順序も、Pythonの独立した全経路列挙と一致した。

- `recursive_firm.json` / `recursive_doctor.json`: 両RDAの全ノード・全分岐・全終点。
- `recursive_firm.md` / `recursive_doctor.md`: 全分岐のMermaid図、各DAの所属、各終点までの削除と移動。
- `problem.json`: 入力選好と定員。
- [verification.json](verification.json): 独立DA検算の結果。
- [input_verification.json](input_verification.json): 画像から転記した入力との照合結果。

5×8の保存形式に合わせ、参考としてIterated DAもseed 42で両告白側を1回ずつ実行し、JSONとMarkdownを保存した。企業告白型は4ノード後に元の問題でUR不安定な終点、個人告白型は2ノード後に元の問題でUR安定な終点に到達した。RDAは乱数を使用しない。

## 再現方法

`MatchingPrograms` を作業ディレクトリとし、出力先には新規ディレクトリを指定する。

```sh
c++ -std=c++17 -O2 -I include \
  experiments/universal_replacement/da_search_trees.cpp \
  src/Matching.cpp src/Utils.cpp src/MatchingUtils.cpp \
  src/algorithms.cpp src/algorithmsUtils.cpp \
  -o /tmp/da_search_trees_difficult_example_1
/tmp/da_search_trees_difficult_example_1 \
  --profile difficult-example-1 --seed 42 \
  --output /tmp/da-trees-difficult-example-1
python3 experiments/universal_replacement/render_da_search_trees.py \
  /tmp/da-trees-difficult-example-1
```

保存済みの結果を再検算し、機械生成の図と比較表を再生成する場合：

```sh
python3 experiments/universal_replacement/render_da_search_trees.py \
  experiments/universal_replacement/tree_docs/recorded_results/difficult_example_1_3x5
```
