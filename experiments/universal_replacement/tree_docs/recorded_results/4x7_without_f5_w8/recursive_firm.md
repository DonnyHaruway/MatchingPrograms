# 企業告白型 Recursive DA（全探索）

[比較と再現方法](README.md)

各枠は完全なDAの終了結果。四角が企業、丸が労働者、枠内の線が所属を表す。線のない点線の労働者は未マッチ。太い矢印は実行した削除、点線矢印は未実行の候補。

P番号はその親ノードの違反ペア一覧の順番。異なるペアから同じ削除を選べる場合も枝を分ける。緑は元の問題でUR安定な終点、赤はUR不安定な終点。N番号はDAの訪問順で、図の左右の配置は探索順を表さない。

```mermaid
flowchart TB
  classDef unmatched fill:#f3f4f6,stroke:#777,stroke-dasharray:4 3;
  classDef placeholder fill:transparent,stroke:transparent,color:transparent;
  classDef unexecuted fill:#fafafa,stroke:#999,color:#666,stroke-dasharray:4 3;
  subgraph n0["N0 / DA1<br/>違反: f1: (w2,w6)"]
    direction TB
    n0f1["f1 (2/2)"]
    n0f2["f2 (2/2)"]
    n0f3["f3 (2/2)"]
    n0f4["f4 (0/2)"]
    n0w1(("w1"))
    n0w2(("w2"))
    n0w3(("w3"))
    n0w4(("w4"))
    n0w5(("w5"))
    class n0w5 unmatched
    n0w6(("w6"))
    n0w7(("w7"))
    n0f1 --- n0w2
    n0f1 --- n0w6
    n0f2 --- n0w1
    n0f2 --- n0w3
    n0f3 --- n0w4
    n0f3 --- n0w7
    n0empty4((" ")):::placeholder
    n0f4 ~~~ n0empty4
  end
  style n0 fill:#fff7e6,stroke:#a66b12,stroke-width:2px
  subgraph n1["N1 / DA2<br/>終点 / 元の問題でUR安定"]
    direction TB
    n1f1["f1 (2/2)"]
    n1f2["f2 (2/2)"]
    n1f3["f3 (2/2)"]
    n1f4["f4 (0/2)"]
    n1w1(("w1"))
    n1w2(("w2"))
    n1w3(("w3"))
    n1w4(("w4"))
    n1w5(("w5"))
    class n1w5 unmatched
    n1w6(("w6"))
    n1w7(("w7"))
    n1f1 --- n1w6
    n1f1 --- n1w7
    n1f2 --- n1w1
    n1f2 --- n1w3
    n1f3 --- n1w2
    n1f3 --- n1w4
    n1empty4((" ")):::placeholder
    n1f4 ~~~ n1empty4
  end
  style n1 fill:#e8f5e9,stroke:#267d3d,stroke-width:2px
  subgraph n2["N2 / DA2<br/>違反: f3: (w4,w5)"]
    direction TB
    n2f1["f1 (2/2)"]
    n2f2["f2 (2/2)"]
    n2f3["f3 (2/2)"]
    n2f4["f4 (1/2)"]
    n2w1(("w1"))
    n2w2(("w2"))
    n2w3(("w3"))
    n2w4(("w4"))
    n2w5(("w5"))
    n2w6(("w6"))
    n2w7(("w7"))
    n2f1 --- n2w2
    n2f1 --- n2w7
    n2f2 --- n2w1
    n2f2 --- n2w3
    n2f3 --- n2w4
    n2f3 --- n2w5
    n2f4 --- n2w6
  end
  style n2 fill:#fff7e6,stroke:#a66b12,stroke-width:2px
  subgraph n3["N3 / DA3<br/>終点 / 元の問題でUR安定"]
    direction TB
    n3f1["f1 (2/2)"]
    n3f2["f2 (2/2)"]
    n3f3["f3 (2/2)"]
    n3f4["f4 (1/2)"]
    n3w1(("w1"))
    n3w2(("w2"))
    n3w3(("w3"))
    n3w4(("w4"))
    n3w5(("w5"))
    n3w6(("w6"))
    n3w7(("w7"))
    n3f1 --- n3w2
    n3f1 --- n3w7
    n3f2 --- n3w3
    n3f2 --- n3w6
    n3f3 --- n3w1
    n3f3 --- n3w5
    n3f4 --- n3w4
  end
  style n3 fill:#e8f5e9,stroke:#267d3d,stroke-width:2px
  subgraph n4["N4 / DA3<br/>終点 / 元の問題でUR安定"]
    direction TB
    n4f1["f1 (2/2)"]
    n4f2["f2 (2/2)"]
    n4f3["f3 (2/2)"]
    n4f4["f4 (0/2)"]
    n4w1(("w1"))
    n4w2(("w2"))
    n4w3(("w3"))
    n4w4(("w4"))
    n4w5(("w5"))
    class n4w5 unmatched
    n4w6(("w6"))
    n4w7(("w7"))
    n4f1 --- n4w2
    n4f1 --- n4w7
    n4f2 --- n4w3
    n4f2 --- n4w6
    n4f3 --- n4w1
    n4f3 --- n4w4
    n4empty4((" ")):::placeholder
    n4f4 ~~~ n4empty4
  end
  style n4 fill:#e8f5e9,stroke:#267d3d,stroke-width:2px
  n0 ==>|"P1: f1からw2を削除"| n1
  n0 ==>|"P1: f1からw6を削除"| n2
  n2 ==>|"P1: f3からw4を削除"| n3
  n2 ==>|"P1: f3からw5を削除"| n4
```

## DAごとの状態

### N0 / DA1

累積削除（順番どおり）: なし。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w2, w6} |
| f2 | {w1, w3} |
| f3 | {w4, w7} |
| f4 | ∅ |
| 未マッチ | {w5} |

違反ペア: P1: f1: (w2,w6)。

### N1 / DA2

累積削除（順番どおり）: (f1,w2)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w6, w7} |
| f2 | {w1, w3} |
| f3 | {w2, w4} |
| f4 | ∅ |
| 未マッチ | {w5} |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: はい。

元の問題でのUR逸脱ペア: なし。

### N2 / DA2

累積削除（順番どおり）: (f1,w6)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w2, w7} |
| f2 | {w1, w3} |
| f3 | {w4, w5} |
| f4 | {w6} |
| 未マッチ | ∅ |

違反ペア: P1: f3: (w4,w5)。

### N3 / DA3

累積削除（順番どおり）: (f1,w6) → (f3,w4)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w2, w7} |
| f2 | {w3, w6} |
| f3 | {w1, w5} |
| f4 | {w4} |
| 未マッチ | ∅ |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: はい。

元の問題でのUR逸脱ペア: なし。

### N4 / DA3

累積削除（順番どおり）: (f1,w6) → (f3,w5)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w2, w7} |
| f2 | {w3, w6} |
| f3 | {w1, w4} |
| f4 | ∅ |
| 未マッチ | {w5} |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: はい。

元の問題でのUR逸脱ペア: なし。

## 各終点までの削除と移動

「残した相方」は削除対象に選ばなかった労働者。以降のDAで所属が維持されるとは限らない。移動だけで不安定とは判定せず、削除した組が終点でUR逸脱ペアになるかを元の選好で別途調べる。

### 終点 N1

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f1: (w2,w6) | f1からw2 | w6 | f1 → f3 → f3 | f1 → f1 → f1 | いいえ |

### 終点 N3

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f1: (w2,w6) | f1からw6 | w2 | f1 → f4 → f2 | f1 → f1 → f1 | いいえ |
| 2 | f3: (w4,w5) | f3からw4 | w5 | f3 → f4 → f4 | f3 → f3 → f3 | いいえ |

### 終点 N4

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f1: (w2,w6) | f1からw6 | w2 | f1 → f4 → f2 | f1 → f1 → f1 | いいえ |
| 2 | f3: (w4,w5) | f3からw5 | w4 | f3 → 未マッチ → 未マッチ | f3 → f3 → f3 | いいえ |
