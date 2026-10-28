# 個人告白型 Recursive DA（全探索）

[比較と再現方法](README.md)

各枠は完全なDAの終了結果。四角が企業、丸が労働者、枠内の線が所属を表す。線のない点線の労働者は未マッチ。太い矢印は実行した削除、点線矢印は未実行の候補。

P番号はその親ノードの違反ペア一覧の順番。異なるペアから同じ削除を選べる場合も枝を分ける。緑は元の問題でUR安定な終点、赤はUR不安定な終点。N番号はDAの訪問順で、図の左右の配置は探索順を表さない。

労働者は削除前の元のラベルで表示する。JSONの添字との対応は入力の `worker_labels_one_based` を参照。

```mermaid
flowchart TB
  classDef unmatched fill:#f3f4f6,stroke:#777,stroke-dasharray:4 3;
  classDef placeholder fill:transparent,stroke:transparent,color:transparent;
  classDef unexecuted fill:#fafafa,stroke:#999,color:#666,stroke-dasharray:4 3;
  subgraph n0["N0 / DA1<br/>違反: f2: (w2,w6)"]
    direction TB
    n0f1["f1 (2/2)"]
    n0f2["f2 (2/2)"]
    n0f3["f3 (2/2)"]
    n0w1(("w1"))
    n0w2(("w2"))
    n0w3(("w3"))
    n0w4(("w4"))
    n0w6(("w6"))
    n0w7(("w7"))
    n0f1 --- n0w1
    n0f1 --- n0w7
    n0f2 --- n0w2
    n0f2 --- n0w6
    n0f3 --- n0w3
    n0f3 --- n0w4
  end
  style n0 fill:#fff7e6,stroke:#a66b12,stroke-width:2px
  subgraph n1["N1 / DA2<br/>終点 / 元の問題でUR不安定"]
    direction TB
    n1f1["f1 (2/2)"]
    n1f2["f2 (2/2)"]
    n1f3["f3 (2/2)"]
    n1w1(("w1"))
    n1w2(("w2"))
    n1w3(("w3"))
    n1w4(("w4"))
    n1w6(("w6"))
    n1w7(("w7"))
    n1f1 --- n1w1
    n1f1 --- n1w2
    n1f2 --- n1w4
    n1f2 --- n1w7
    n1f3 --- n1w3
    n1f3 --- n1w6
  end
  style n1 fill:#ffebee,stroke:#b32626,stroke-width:2px
  subgraph n2["N2 / DA2<br/>終点 / 元の問題でUR安定"]
    direction TB
    n2f1["f1 (2/2)"]
    n2f2["f2 (2/2)"]
    n2f3["f3 (2/2)"]
    n2w1(("w1"))
    n2w2(("w2"))
    n2w3(("w3"))
    n2w4(("w4"))
    n2w6(("w6"))
    n2w7(("w7"))
    n2f1 --- n2w1
    n2f1 --- n2w6
    n2f2 --- n2w2
    n2f2 --- n2w7
    n2f3 --- n2w3
    n2f3 --- n2w4
  end
  style n2 fill:#e8f5e9,stroke:#267d3d,stroke-width:2px
  n0 ==>|"P1: f2からw2を削除"| n1
  n0 ==>|"P1: f2からw6を削除"| n2
```

## DAごとの状態

### N0 / DA1

累積削除（順番どおり）: なし。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w1, w7} |
| f2 | {w2, w6} |
| f3 | {w3, w4} |
| 未マッチ | ∅ |

違反ペア: P1: f2: (w2,w6)。

### N1 / DA2

累積削除（順番どおり）: (f2,w2)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w1, w2} |
| f2 | {w4, w7} |
| f3 | {w3, w6} |
| 未マッチ | ∅ |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: いいえ。

元の問題でのUR逸脱ペア: (f2,w2)。

### N2 / DA2

累積削除（順番どおり）: (f2,w6)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w1, w6} |
| f2 | {w2, w7} |
| f3 | {w3, w4} |
| 未マッチ | ∅ |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: はい。

元の問題でのUR逸脱ペア: なし。

## 各終点までの削除と移動

「残した相方」は削除対象に選ばなかった労働者。以降のDAで所属が維持されるとは限らない。移動だけで不安定とは判定せず、削除した組が終点でUR逸脱ペアになるかを元の選好で別途調べる。

### 終点 N1

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f2: (w2,w6) | f2からw2 | w6 | f2 → f1 → f1 | f2 → f3 → f3 | はい |

### 終点 N2

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f2: (w2,w6) | f2からw6 | w2 | f2 → f1 → f1 | f2 → f2 → f2 | いいえ |
