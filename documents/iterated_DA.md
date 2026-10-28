# DA完了後に同僚の拒否ペアを削除して再実行するアルゴリズム

実行経路と未選択の削除候補を描く履歴API、およびRecursive DAの全探索は
[削除分岐木の仕様・例・実行方法](../experiments/universal_replacement/tree_docs/README.md) を参照。

個人提案版は `iterated_doctor_proposing_DA_algorithm`、企業提案版は
`iterated_firm_proposing_DA_algorithm`。次のA・B・Cを繰り返す。

## A：通常のDAを最後まで実行

全員を未マッチにし、提案履歴も初期化する。同僚への選好を無視して、
個人提案版では `doctor_proposing_DA_algorithm`、企業提案版では
`firm_proposing_DA_algorithm` を終了まで実行する。
双方の負の選好スコアは受け入れ不可、`0` は受け入れ可能。定員0の企業にはマッチしない。
同順位の扱いは各通常DAに従う。

## B：拒否ペアを1組選んで片方を削除

1. 完了したDAの結果から、同じ企業に所属する個人の拒否ペアを全企業分集める。
   どちらか一方向でも同僚選好が `-MatchingTypes::INF` なら拒否ペアとする。
2. 拒否ペアがなければ、そのマッチングを返して終了する。
3. 全拒否ペアの中から**1組だけを一様ランダムに選ぶ**。
4. その組の片方を等確率（1/2）で選び、該当企業からその個人への選好を `-INF` にする。

削除するのはその企業との関係だけ。他の企業の選好、個人の企業への選好、同僚選好は変更しない。

## C：更新した選好を保持してAへ戻る

企業選好のコピーをメモリ上で保持し、過去の削除を累積する。
ファイル保存は行わず、呼び出し元の入力も変更しない。
再実行時は前回のマッチングや提案履歴を引き継がず、**DAを最初から実行する**。
これにより、前回拒否された個人も、空席ができた企業に再び提案できる。

毎回、それまで削除されていなかった企業・個人の組を1つ削除するため、
削除回数は高々 `n_agents * n_firms`、DAの実行回数はその値に1を加えた回数。
返り値は `Matching`。更新した選好は実行内だけで使う。
元の選好に対する安定性は、この終了条件とは別に判定する。
`MatchingSystem::run_algorithm` が計算するスコアも元の入力選好に基づく。

## 使用例

```cpp
// ms に選好と定員を設定済みとする。42 は再現用の乱数seed。
Matching doctor = ms.run_algorithm("iterated_doctor_propose_DA", 42);
Matching firm = ms.run_algorithm("iterated_firm_propose_DA", 42);
```

直接呼び出す場合：

```cpp
Matching doctor = iterated_doctor_proposing_DA_algorithm(
    n_agents, n_firms, firm_capacities,
    agent_prefs, firm_prefs, agent_col_prefs, 42);

Matching firm = iterated_firm_proposing_DA_algorithm(
    n_agents, n_firms, firm_capacities,
    agent_prefs, firm_prefs, agent_col_prefs, 42);
```

seedを省略すると `std::random_device` から取得する。
同じ入力・seed・実行環境では同じ結果になる。

既存の `DARandomChoiceSelector` を最後の引数に指定すると、乱択の選択肢を直接制御できる。
`pair_order` は複数の拒否ペアから次に処理する1組の選択、
`removal_endpoint` はその組の削除対象（`0=left`、`1=right`）を表す。
拒否ペアが1組なら `pair_order` は呼び出さない。
`DARandomChoice::round` は1から始まるDA実行回数、`agent_match` は削除前の完了したDAの結果。
選択範囲外の値を返すと `std::out_of_range` を送出する。

## 削除対象を決める派生版

以下の4関数は、Bの「ペアのどちらを削除するか」だけを変更する。
全企業の拒否ペアから1組を選ぶ処理は引き続き一様ランダム。
1組の片方を削除したら、累積した削除を保持してDAを最初から実行する。
引数・返り値は既存の `iterated_*_proposing_DA_algorithm` と同じ。

| 削除規則 | 個人提案版 | 企業提案版 |
| --- | --- | --- |
| 履歴優先 | `iterated_doctor_proposing_DA_history_priority_algorithm` | `iterated_firm_proposing_DA_history_priority_algorithm` |
| 企業選好優先 | `iterated_doctor_proposing_DA_firm_preference_algorithm` | `iterated_firm_proposing_DA_firm_preference_algorithm` |

### 履歴優先

企業・個人の組ごとに、選ばれた拒否ペアを解消する際に初めて「削除せず残す」と
判断したDA反復番号を記録する。単にDAでマッチしただけの個人や、
列挙されたが処理対象に選ばれていないペアには履歴を付けない。

- 片方だけに履歴があれば、その個人を残してもう片方を削除する。
- 両方に履歴があれば、初めて残された反復が早い方を残す。
- 両方に履歴がなければ、片方を等確率（1/2）で削除する。

残すと判断した個人に履歴がなければ、現在の反復番号を記録する。
すでに履歴があれば上書きしない。1反復で新たに記録する個人は高々1人のため、
異なる個人の初回反復番号が同じになることはない。
別の企業で残された履歴は使わず、履歴は関数呼び出しごとに初期化する。
履歴があっても、通常DA内で定員や企業の選好によって拒否されることはある。

履歴は配列で保持するため、選んだ1組の削除判断・履歴更新は `O(1)`。
履歴配列の追加メモリと初期化時間は `O(n_firms * n_agents)`。
DAの実行と拒否ペアの列挙にかかる時間は別途必要。

`choice_selector` はペアの乱択と、両方に履歴がない場合の削除対象の乱択だけに使う。
履歴から削除対象が決まる場合、`removal_endpoint` は呼ばない。

### 企業選好優先

選んだ拒否ペアの2人について、企業の選好スコアが低い方を削除する。
スコアは大きいほど好ましい。同点の場合は個人IDが大きい方を削除する。
どちらが相手を拒否しているかは、削除対象の判断に使わない。
選んだ1組の削除判断は `O(1)`。

`choice_selector` は複数ペアからの1組の乱択だけに使い、
`removal_endpoint` は呼ばない。ペア選択はランダムなので、
この方式も一般にはseedによって最終結果が変わりうる。

### MatchingSystemからの呼び出し

```cpp
Matching history_doctor = ms.run_algorithm("iterated_doctor_propose_DA_history_priority", 42);
Matching history_firm = ms.run_algorithm("iterated_firm_propose_DA_history_priority", 42);
Matching preference_doctor = ms.run_algorithm("iterated_doctor_propose_DA_firm_preference", 42);
Matching preference_firm = ms.run_algorithm("iterated_firm_propose_DA_firm_preference", 42);
```
