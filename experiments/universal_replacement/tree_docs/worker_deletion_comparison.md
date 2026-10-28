# 3×7問題から労働者を1人ずつ除いた比較

実行日: 2026-10-21。

共通の出発点は [f4・f5・w8を除いた3×7問題](recorded_results/3x7_without_f4_f5_w8/problem.json)。以下はそれぞれ、この3×7問題から労働者を1人だけ除いた独立ケース。残る企業の定員・選好スコア・相対順位を維持した。「UR安定」は各ケースの再帰的削除を行う前の選好に対する判定。

## 個人告白型Recursive DA

| 除いた労働者 | DAノード | 終点 | UR安定な終点 | 分岐図 |
|---|---:|---:|---:|---|
| なし（元の3×7） | 7 | 4 | 0 | [元の問題](recorded_results/3x7_without_f4_f5_w8/recursive_doctor.md) |
| w4 | 1 | 1 | 1 | [w4削除](recorded_results/3x6_without_f4_f5_w4_w8/recursive_doctor.md) |
| w5 | 3 | 2 | 1 | [w5削除](recorded_results/3x6_without_f4_f5_w5_w8/recursive_doctor.md) |
| w6 | 1 | 1 | 1 | [w6削除](recorded_results/3x6_without_f4_f5_w6_w8/recursive_doctor.md) |

w4削除・w6削除では、初回の通常DAがすでにUR安定で、削除分岐は発生しない。全員マッチしている。

| ケース | f1 | f2 | f3 |
|---|---|---|---|
| w4削除 | {w1,w5} | {w2,w7} | {w3,w6} |
| w6削除 | {w1,w5} | {w2,w7} | {w3,w4} |

## 企業告白型Recursive DA

| 除いた労働者 | DAノード | 終点 | UR安定な終点 | 分岐図 |
|---|---:|---:|---:|---|
| なし（元の3×7） | 5 | 3 | 3 | [元の問題](recorded_results/3x7_without_f4_f5_w8/recursive_firm.md) |
| w4 | 5 | 4 | 4 | [w4削除](recorded_results/3x6_without_f4_f5_w4_w8/recursive_firm.md) |
| w5 | 3 | 2 | 2 | [w5削除](recorded_results/3x6_without_f4_f5_w5_w8/recursive_firm.md) |
| w6 | 3 | 2 | 1 | [w6削除](recorded_results/3x6_without_f4_f5_w6_w8/recursive_firm.md) |

終点数は削除履歴を区別した数であり、同じマッチングに至る経路も別々に数える。

Pythonの独立DAモデルで全ノード・全削除経路・UR判定を照合済み。Markdownでは元の労働者ラベルを維持している。

[w4削除の実験条件・再現方法](recorded_results/3x6_without_f4_f5_w4_w8/experiment.md) / [w6削除の実験条件・再現方法](recorded_results/3x6_without_f4_f5_w6_w8/experiment.md)
