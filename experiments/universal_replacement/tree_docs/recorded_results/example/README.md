# DAの削除分岐木

[実装仕様・実行方法](../../README.md)

UR安定性は削除前の元の選好に対して判定し、空席への追加逸脱を含む。Recursive DAは全ての違反ペア・両削除対象を探索する。禁止ペアがなくなった枝は終了するが、他の枝は探索を続け、重複する履歴も残す。

| 実行 | DAノード | 終点 | 元の問題でUR安定な終点 | 図と移動履歴 |
|---|---:|---:|---:|---|
| iterated_firm | 2 | 1 | 1 | [iterated_firm](iterated_firm.md) |
| iterated_doctor | 2 | 1 | 1 | [iterated_doctor](iterated_doctor.md) |
| recursive_firm | 5 | 3 | 1 | [recursive_firm](recursive_firm.md) |
| recursive_doctor | 3 | 2 | 1 | [recursive_doctor](recursive_doctor.md) |

Pythonの独立DAモデルで全ノードの割当・違反ペア・累積削除・移動・元/削除後のUR判定を照合した。Recursive DAの全終点と削除順序は、独立した全経路列挙とも一致した。

[照合結果](verification.json) / [入力選好](problem.json)
