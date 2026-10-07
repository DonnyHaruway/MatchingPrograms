# DA完了後に同僚の拒否ペアを削除して再実行するアルゴリズム

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
