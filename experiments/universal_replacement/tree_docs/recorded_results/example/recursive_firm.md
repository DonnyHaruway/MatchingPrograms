# 企業告白型 Recursive DA（全探索）

[比較と再現方法](README.md)

各枠は完全なDAの終了結果。四角が企業、丸が労働者、枠内の線が所属を表す。線のない点線の労働者は未マッチ。太い矢印は実行した削除、点線矢印は未実行の候補。

P番号はその親ノードの違反ペア一覧の順番。異なるペアから同じ削除を選べる場合も枝を分ける。緑は元の問題でUR安定な終点、赤はUR不安定な終点。N番号はDAの訪問順で、図の左右の配置は探索順を表さない。

```mermaid
flowchart TB
  classDef unmatched fill:#f3f4f6,stroke:#777,stroke-dasharray:4 3;
  classDef placeholder fill:transparent,stroke:transparent,color:transparent;
  classDef unexecuted fill:#fafafa,stroke:#999,color:#666,stroke-dasharray:4 3;
  subgraph n0["N0 / DA1<br/>違反: f1: (w1,w2)"]
    direction TB
    n0f1["f1 (2/2)"]
    n0f2["f2 (1/1)"]
    n0w1(("w1"))
    n0w2(("w2"))
    n0w3(("w3"))
    class n0w3 unmatched
    n0w4(("w4"))
    n0f1 --- n0w1
    n0f1 --- n0w2
    n0f2 --- n0w4
  end
  style n0 fill:#fff7e6,stroke:#a66b12,stroke-width:2px
  subgraph n1["N1 / DA2<br/>終点 / 元の問題でUR安定"]
    direction TB
    n1f1["f1 (2/2)"]
    n1f2["f2 (1/1)"]
    n1w1(("w1"))
    class n1w1 unmatched
    n1w2(("w2"))
    n1w3(("w3"))
    n1w4(("w4"))
    n1f1 --- n1w2
    n1f1 --- n1w3
    n1f2 --- n1w4
  end
  style n1 fill:#e8f5e9,stroke:#267d3d,stroke-width:2px
  subgraph n2["N2 / DA2<br/>違反: f1: (w1,w3)"]
    direction TB
    n2f1["f1 (2/2)"]
    n2f2["f2 (1/1)"]
    n2w1(("w1"))
    n2w2(("w2"))
    class n2w2 unmatched
    n2w3(("w3"))
    n2w4(("w4"))
    n2f1 --- n2w1
    n2f1 --- n2w3
    n2f2 --- n2w4
  end
  style n2 fill:#fff7e6,stroke:#a66b12,stroke-width:2px
  subgraph n3["N3 / DA3<br/>終点 / 元の問題でUR不安定"]
    direction TB
    n3f1["f1 (2/2)"]
    n3f2["f2 (1/1)"]
    n3w1(("w1"))
    n3w2(("w2"))
    class n3w2 unmatched
    n3w3(("w3"))
    n3w4(("w4"))
    n3f1 --- n3w3
    n3f1 --- n3w4
    n3f2 --- n3w1
  end
  style n3 fill:#ffebee,stroke:#b32626,stroke-width:2px
  subgraph n4["N4 / DA3<br/>終点 / 元の問題でUR不安定"]
    direction TB
    n4f1["f1 (1/2)"]
    n4f2["f2 (1/1)"]
    n4w1(("w1"))
    n4w2(("w2"))
    class n4w2 unmatched
    n4w3(("w3"))
    class n4w3 unmatched
    n4w4(("w4"))
    n4f1 --- n4w4
    n4f2 --- n4w1
  end
  style n4 fill:#ffebee,stroke:#b32626,stroke-width:2px
  n0 ==>|"P1: f1からw1を削除"| n1
  n0 ==>|"P1: f1からw2を削除"| n2
  n2 ==>|"P1: f1からw1を削除"| n3
  n2 ==>|"P1: f1からw3を削除"| n4
```

## DAごとの状態

### N0 / DA1

累積削除（順番どおり）: なし。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w1, w2} |
| f2 | {w4} |
| 未マッチ | {w3} |

違反ペア: P1: f1: (w1,w2)。

### N1 / DA2

累積削除（順番どおり）: (f1,w1)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w2, w3} |
| f2 | {w4} |
| 未マッチ | {w1} |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: はい。

元の問題でのUR逸脱ペア: なし。

### N2 / DA2

累積削除（順番どおり）: (f1,w2)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w1, w3} |
| f2 | {w4} |
| 未マッチ | {w2} |

違反ペア: P1: f1: (w1,w3)。

### N3 / DA3

累積削除（順番どおり）: (f1,w2) → (f1,w1)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w3, w4} |
| f2 | {w1} |
| 未マッチ | {w2} |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: いいえ。

元の問題でのUR逸脱ペア: (f1,w2)。

### N4 / DA3

累積削除（順番どおり）: (f1,w2) → (f1,w3)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w4} |
| f2 | {w1} |
| 未マッチ | {w2, w3} |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: いいえ。

元の問題でのUR逸脱ペア: (f1,w2), (f1,w3)。

## 各終点までの削除と移動

「残した相方」は削除対象に選ばなかった労働者。以降のDAで所属が維持されるとは限らない。移動だけで不安定とは判定せず、削除した組が終点でUR逸脱ペアになるかを元の選好で別途調べる。

### 終点 N1

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f1: (w1,w2) | f1からw1 | w2 | f1 → 未マッチ → 未マッチ | f1 → f1 → f1 | いいえ |

### 終点 N3

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f1: (w1,w2) | f1からw2 | w1 | f1 → 未マッチ → 未マッチ | f1 → f1 → f2 | はい |
| 2 | f1: (w1,w3) | f1からw1 | w3 | f1 → f2 → f2 | f1 → f1 → f1 | いいえ |

### 終点 N4

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f1: (w1,w2) | f1からw2 | w1 | f1 → 未マッチ → 未マッチ | f1 → f1 → f2 | はい |
| 2 | f1: (w1,w3) | f1からw3 | w1 | f1 → 未マッチ → 未マッチ | f1 → f2 → f2 | はい |
