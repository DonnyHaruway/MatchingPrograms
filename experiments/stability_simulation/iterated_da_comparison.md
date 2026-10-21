# Iterated DAの安定性比較（企業5・個人8）

ランダム削除・履歴優先削除・企業選好による削除の3方式について、個人提案版と企業提案版の計6アルゴリズムを比較する。
同じ選好プロファイルを6方式すべてに入力し、各出力を元の選好に対して独立に判定する。

- weak stability：既存の `Matching::is_weakly_stable`
- universal replacement stability：既存の `Matching::is_universal_replacement_stable`

通常の stability は調べない。安定なマッチングの存在を全探索する実験でもない。
割合の分母は、各Kについて生成した全プロファイル数。

## ビルド・実行

`programs` をカレントディレクトリにして実行する。

```sh
cmake -S . -B build
cmake --build build --target compare_iterated_da_5x8 test_iterated_da_comparison -j4
ctest --test-dir build --output-on-failure -R 'IteratedDAComparison'

# 定員 [2,2,2,2,2]、拒否ペア7組、共通の10,000プロファイル
./build/experiments/stability_simulation/compare_iterated_da_5x8

# 拒否ペア数だけを変更して比較する。各Kにつき10,000件。
./build/experiments/stability_simulation/compare_iterated_da_5x8 \
  --conflict-pairs 0,7,14,21,28 --trials 10000 --seed 20261014

# K=0..28の全値を比較する。29 × 10,000プロファイル。
./build/experiments/stability_simulation/compare_iterated_da_5x8 \
  --conflict-pairs all --summary-only
```

既存の `simulate_matching_stability` とは別の実行ファイル。
実行ごとに新しい出力ディレクトリを作成する。

| オプション | 初期値 | 内容 |
| --- | --- | --- |
| `--trials N` | `10000` | 各Kで生成する共通プロファイル数。6方式で計 `6*N` 回実行 |
| `--conflict-pairs K[,K...]` | `7` | 正確な拒否ペア数。0〜28の整数、カンマ区切り、または `all` |
| `--capacities C,C,C,C,C` | `2,2,2,2,2` | 5企業の定員。各値は0以上 |
| `--seed N` | `20261014` | 選好とアルゴリズムの乱択を再現する基準seed |
| `--colleague-mode directed\|symmetric` | `directed` | 一方向の拒否を含めるか、常に相互拒否にするか |
| `--firm-scores linear\|superincreasing` | `linear` | 企業のスコア尺度。既存実験と同じ変換 |
| `--output DIR` | `experiments/stability_simulation/outputs/iterated_da_5x8` | 出力先の親ディレクトリ |
| `--summary-only` | 指定なし | プロファイルごとのJSONL保存を省略 |

## 選好の生成と「違反ペア数」

個人の企業への選好は1〜5、企業の個人への選好は1〜8を、それぞれ独立に一様ランダムな順列として生成する。
すべての企業・個人の関係を受け入れ可能とし、同順位は設けない。
同僚選好は、自己への値を0、他者への値を0または `-INF` とする。

設定するKは、8人の全28組のうち、少なくとも一方向の同僚選好が `-INF` である**順序を区別しない組の数**。
相互拒否でも1組と数える。拒否する方向の数（最大56）とは異なる。

28組を一様ランダムに並べ、先頭K組だけを拒否ペアにする。
`directed` では、選んだ組ごとに「左だけが拒否」「右だけが拒否」「双方が拒否」の3通りから等確率で選ぶ。
これにより、拒否ペアが正確にK組である二値同僚選好行列を一様に生成する。
`symmetric` では、選んだ組を常に相互拒否にする。

Kはマッチング前の選好に含まれる拒否ペア数であり、最初のDA結果で実際に同じ企業に所属した違反ペアの数を直接固定するものではない。
その実際の数は `initial_da_conflict_pairs` として別途記録する。
各方式の最終出力に違反ペアが残っていたり定員を超えていたりした場合は、実験をエラーで終了する。

同じ `(seed, trial)` では、Kや実行順に依存せず企業・個人の順位が同じになる。
Kを増やすと拒否ペアは同じランダム順序の先頭から追加され、既存の拒否方向も維持される。
したがって、Kを変えた実験でも、順位を保ったまま拒否の増加を比較できる。
プロファイルは復元抽出で、重複の排除は行わない。

アルゴリズムseedは基準seed・trial・提案側から決定する。
同じ提案側の3方式に同じseedを渡すが、必要な乱数の数が異なるため、全反復で同じペアを処理するとは限らない。
各プロファイルで各方式を1回実行する。乱択の全経路を列挙する実験ではない。
同じ入力・seed・実行環境では選好、マッチング、判定、集計数を再現できる。測定時間は変動する。

## 出力

実行ディレクトリの `summary.csv` に、すべての `(K, アルゴリズム)` の集計をまとめる。
各 `k7/` などのサブディレクトリには以下を保存する。

- `summary.txt`：実験条件、2種類の安定性の件数・百分率、両方を満たす割合、平均マッチ人数、初回DAの平均違反ペア数
- `summary.csv`：同じ集計の機械可読データ。割合は0〜1
- `by_initial_conflicts.csv`：最初のDA結果の違反ペア数ごとの件数・安定性割合
- `instances.jsonl`：各プロファイルの3種類の選好、定員、K、6方式の出力と独立した2種類の判定、アルゴリズムseed。`--summary-only` では作成しない

JSONLの `agent_match` は個人ID順の所属企業ID（いずれも0始まり）で、未マッチは `MatchingTypes::UNMATCHED`。
`algorithm_seconds` はDAアルゴリズム実行部分のみの時間で、安定性の判定やファイル保存の時間を含まない。
小さいプロファイルでのスレッド生成の負担を避けるため、この実行ファイル内ではOpenMPのスコア計算を1スレッドに設定する。

universal replacement stability は、満員の企業に対する置換に加えて、空席への追加によるブロッキングも調べる。
空席の場合は、個人が移動先を現在より好み、企業がその個人の追加を好み（企業選好スコアが正）、
追加後にどちらの方向にも同僚の拒否がない場合にブロッキングとなる。
満員の場合は、企業が候補者を少なくとも1人の在籍者より好み、誰と置換しても同僚の拒否がない場合を対象とする。
CSV・JSONL・集計に `vacancy_blocking=true` を記録する。2026-10-14の修正前の出力はこの条件を含まない旧判定の記録であり、現在の判定で再実行する必要がある。

## Universal replacement stabilityを満たさなかったケースの抽出

保存済みの結果から抽出できる。アルゴリズムを再実行する必要はない。

```sh
python3 experiments/stability_simulation/extract_universal_replacement_failures.py \
  experiments/stability_simulation/outputs/iterated_da_5x8/run_YYYYMMDD_HHMMSS_seed20261014/k7/instances.jsonl
```

入力と同じディレクトリに `universal_replacement_failures/` を作成し、
少なくとも1方式が判定を満たさなかったプロファイルを `profiles.jsonl` に、
各方式が満たさなかったプロファイルを方式名ごとのJSONLに保存する。
元の選好・定員・全6方式の出力も保持する。
`index.csv` は失敗した方式ごとに1行、`summary.md` は件数と最初のケースを表示する。
`--output DIR` で保存先を指定できる。既存の保存先がある場合は番号を付けた新規ディレクトリを使う。
`--all-algorithms` を指定すると、全方式の出力が安定性を満たさなかった選好だけを抽出する。

## 指定した選好について安定マッチングの存在を全探索

```sh
cmake --build build --target exhaustive_iterated_da_profile -j4
./build/experiments/stability_simulation/exhaustive_iterated_da_profile \
  --trial 8198 --seed 20261014 --conflict-pairs 7 --capacities 2,2,2,2,2
```

`--trial` は比較実験の0始まりの試行番号。seed・K・定員・同僚選好モード・企業スコア尺度を比較実験と一致させる。
同じ生成器で選好を再現し、未マッチを含む定員内の全割当を、途中で打ち切らずに既存の universal replacement stability 判定にかける。
人数別の列挙件数・安定件数・最初の安定な配置を `summary.txt` に、全8人をマッチさせる安定配置をすべて `all_8_matched_stable_matchings.csv` に保存する。
`profile_fingerprint.txt` は3種類の選好行列を記録し、保存済みのプロファイルとの一致確認に使える。
空席への追加もブロッキングに含むため、全員未マッチでも受け入れ可能な企業・個人の組があれば不安定になる。
存在の確認に加えて、全員マッチの安定配置の有無を別に示す。
