# 企業告白型 Iterated DA（1回の実行）

[比較と再現方法](README.md)

各枠は完全なDAの終了結果。四角が企業、丸が労働者、枠内の線が所属を表す。線のない点線の労働者は未マッチ。太い矢印は実行した削除、点線矢印は未実行の候補。

P番号はその親ノードの違反ペア一覧の順番。異なるペアから同じ削除を選べる場合も枝を分ける。緑は元の問題でUR安定な終点、赤はUR不安定な終点。N番号はDAの訪問順で、図の左右の配置は探索順を表さない。

労働者は削除前の元のラベルで表示する。JSONの添字との対応は入力の `worker_labels_one_based` を参照。

乱数seed: `42`。未実行の枝についてDA結果は計算していない。

```mermaid
flowchart TB
  classDef unmatched fill:#f3f4f6,stroke:#777,stroke-dasharray:4 3;
  classDef placeholder fill:transparent,stroke:transparent,color:transparent;
  classDef unexecuted fill:#fafafa,stroke:#999,color:#666,stroke-dasharray:4 3;
  subgraph n0["N0 / DA1<br/>違反: f3: (w4,w5)"]
    direction TB
    n0f1["f1 (2/2)"]
    n0f2["f2 (2/2)"]
    n0f3["f3 (2/2)"]
    n0w1(("w1"))
    n0w2(("w2"))
    n0w3(("w3"))
    n0w4(("w4"))
    n0w5(("w5"))
    n0w7(("w7"))
    n0f1 --- n0w2
    n0f1 --- n0w7
    n0f2 --- n0w1
    n0f2 --- n0w3
    n0f3 --- n0w4
    n0f3 --- n0w5
  end
  style n0 fill:#fff7e6,stroke:#a66b12,stroke-width:2px
  subgraph n1["N1 / DA2<br/>終点 / 元の問題でUR不安定"]
    direction TB
    n1f1["f1 (2/2)"]
    n1f2["f2 (2/2)"]
    n1f3["f3 (1/2)"]
    n1w1(("w1"))
    n1w2(("w2"))
    n1w3(("w3"))
    n1w4(("w4"))
    class n1w4 unmatched
    n1w5(("w5"))
    n1w7(("w7"))
    n1f1 --- n1w1
    n1f1 --- n1w5
    n1f2 --- n1w2
    n1f2 --- n1w7
    n1f3 --- n1w3
  end
  style n1 fill:#ffebee,stroke:#b32626,stroke-width:2px
  n0 ==>|"P1: f3からw4を削除"| n1
  u1["未実行"]:::unexecuted
  n0 -.->|"P1: f3からw5を削除"| u1
```

## DAごとの状態

### N0 / DA1

累積削除（順番どおり）: なし。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w2, w7} |
| f2 | {w1, w3} |
| f3 | {w4, w5} |
| 未マッチ | ∅ |

違反ペア: P1: f3: (w4,w5)。

### N1 / DA2

累積削除（順番どおり）: (f3,w4)。

| 企業 | 所属労働者 |
|---|---|
| f1 | {w1, w5} |
| f2 | {w2, w7} |
| f3 | {w3} |
| 未マッチ | {w4} |

違反ペア: なし。

削除後の選好でUR安定: はい。 元の選好でUR安定: いいえ。

元の問題でのUR逸脱ペア: (f3,w4)。

## 各終点までの削除と移動

「残した相方」は削除対象に選ばなかった労働者。以降のDAで所属が維持されるとは限らない。移動だけで不安定とは判定せず、削除した組が終点でUR逸脱ペアになるかを元の選好で別途調べる。

### 終点 N1

| 順番 | 選択違反ペア | 削除 | 残した相方 | 削除側：親→直後→終点 | 相方：親→直後→終点 | 削除組が終点のUR逸脱 |
|---|---|---|---|---|---|---|
| 1 | f3: (w4,w5) | f3からw4 | w5 | f3 → 未マッチ → 未マッチ | f3 → f1 → f1 | はい |
