# RDAの分割実行管理と結果統合

`manage_rda_exhaustive.py`は、1つの市場・禁止ペア数について代表入力IDの区間を生成し、
指定並列数までワーカーを起動する。完了した枠に次の区間を割り当て、全区間が完了したら
既存形式の集計・証拠例を `merged/` に統合する。Python標準ライブラリのみを使う。
企業告白型RDA・全分岐探索・元問題に対する3安定性・全割当の存在調査は変更しない。

## サーバーでの実行

`MatchingPrograms`直下で実行する。初回はReleaseビルドを用意する。

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target run_rda_exhaustive -j16

RDA_MANAGER=experiments/universal_replacement/manage_rda_exhaustive.py
RDA_OUTPUT=experiments/universal_replacement/outputs/managed_3x4_k1

# 計画だけ作る。RDAはまだ実行しない。
python3 "$RDA_MANAGER" plan \
  --firms 3 --workers 4 --capacities 1,1,2 --forbidden-pairs 1 \
  --chunk-size 100000000 \
  --output "$RDA_OUTPUT"

# 計画した全区間を最大32並列で実行し、自動統合する。
python3 "$RDA_MANAGER" run --output "$RDA_OUTPUT" --jobs 32
```

この条件は総代表数71,663,616,000、1億代表ずつ717区間。
最終区間は `[71600000000,71663616000)`、63,616,000代表となる。
禁止ペア数2・3はそれぞれ別の出力先で計画・実行する。
`--binary`を省略すると、リポジトリの `build-release/experiments/universal_replacement/run_rda_exhaustive` と、
その隣の `rda_stability_engine` を使う。別ビルドなら `plan --binary /absolute/path/to/run_rda_exhaustive` を指定する。

新しい出力先なら、`run`に市場条件を渡して計画生成からまとめて実行することもできる。

```bash
python3 "$RDA_MANAGER" run \
  --firms 3 --workers 4 --capacities 1,1,2 --forbidden-pairs 1 \
  --chunk-size 100000000 --jobs 32 \
  --output experiments/universal_replacement/outputs/managed_3x4_k1_another
```

長時間実行では `tmux` 等の中で起動する。
計画生成後なら、管理側のログも同じ出力先へ保存できる。

```bash
python3 "$RDA_MANAGER" run --output "$RDA_OUTPUT" --jobs 32 \
  > "$RDA_OUTPUT/manager.log" 2>&1
```

## 区間と計画の検査

`chunks.csv`には以下の列を保存する。

| 列 | 意味 |
|---|---|
| `chunk_id` | 0始まりの区間番号 |
| `start_id` | 最初の代表入力ID |
| `end_id_exclusive` | 含まない終端ID |
| `count` | 区間の代表数 |
| `output_directory` | 管理ディレクトリからの相対出力先 |

区間サイズは固定し、最後だけ端数にする。各入力はIDから直接復元するため空走査は不要。
`plan.json`は市場条件・列挙方式・重み・実装のハッシュ・実行範囲・区間サイズ・
区間数・マニフェストのハッシュを保持する。計画自体にもチェックサムを付ける。
再開時に、先頭・隣接区間・終端・件数・全行の内容を再検査する。
生成した計画や区間表は編集しない。

既定のマニフェスト上限は100,000区間。超える場合は入力を切り捨てずエラーとする。
`--chunk-size`を増やすか、意図的に `--plan-limit` を指定する。
`--dry-run`は出力先を作らず件数を確認する。

```bash
python3 "$RDA_MANAGER" plan \
  --firms 3 --workers 4 --capacities 1,1,2 --forbidden-pairs 1 \
  --output /tmp/unused_rda_plan --dry-run
```

## 主なオプション

| コマンド・引数 | 意味 |
|---|---|
| `plan` | 新規計画のみ生成。既存出力先には上書きしない |
| `run` | 計画を作成、または既存計画を再開。完了済み区間をスキップ |
| `status` | 実際の区間ファイルとロックから進捗を検査 |
| `merge` | 完了した全区間を検査して統合 |
| `--chunk-size N` | 1区間の代表数。計画生成時の既定1億 |
| `--jobs N` | 最大同時実行数。既定32。再開時に変更可 |
| `--progress-every N` | 各ワーカーの途中保存間隔。計画生成時の既定10万 |
| `--status-every SECONDS` | 管理側の進捗保存間隔。既定10秒 |
| `--start-id ID --count N` | 全領域ではなく指定範囲の計画を作る |
| `--stop-after-chunks N` | 今回の起動で最大N区間だけ実行し、全て終了してから停止 |
| `--no-merge` | 全区間完了後の自動統合を省略 |
| `--write-profiles` | 計画生成時に全入力別CSVの保存を有効化 |
| `--merge-profiles` | 統合時に全入力別CSVも連結。既定は集計・証拠のみ |
| `--shutdown-timeout SECONDS` | 停止要求後の保存待ち時間。既定60秒 |

`--write-profiles`は巨大な出力になるため、標準では無効。
有効な場合も各区間のCSVはそのまま保存し、全件連結は明示指定時のみ行う。
`--merge-profiles`で連結するには、計画を `--write-profiles` 付きで作る必要がある。
自動統合後に統合オプションを変えて同じ `merged/` を上書きすることはしない。

## 出力

```text
managed_3x4_k1/
├── plan.json
├── chunks.csv
├── manager_status.json
├── logs/
│   ├── part_000000.log
│   ├── part_000000.attempt.json
│   └── ...
├── chunks/
│   ├── part_000000/       # 既存CLIと同じconfig/CSV/証拠/チェックポイント/完了記録
│   ├── part_000001/
│   └── ...
├── merged/               # 全区間完了後に作成
│   ├── config.json
│   ├── summary.csv
│   ├── profile_classes.csv
│   ├── leaf_patterns.csv
│   ├── search_stats.csv
│   ├── examples.jsonl
│   ├── checkpoint.json
│   ├── progress.json
│   ├── completion.json
│   └── merge_manifest.json
└── .manager.lock
```

管理ログは追記し、再試行の時刻・起動引数・PIDも残す。
`manager_status.json`には完了区間数、稼働中区間とPID、確定した処理代表数、
最近の合計処理速度、残り時間の概算、失敗した区間を保存する。
速度と残り時間は各ワーカーのチェックポイント更新間隔の影響を受ける概算。
`status --json`では管理側の最終状態も `last_manager_status` に表示する。

```bash
python3 "$RDA_MANAGER" status --output "$RDA_OUTPUT"
python3 "$RDA_MANAGER" status --output "$RDA_OUTPUT" --json
tail -f "$RDA_OUTPUT/logs/part_000000.log"
```

## 停止・再開・失敗

Ctrl+C / SIGTERM / SIGHUPで新規起動を止め、稼働中ワーカーにSIGTERMを送り、
入力処理とチェックポイントの保存を待つ。時間切れなら既知のワーカーを強制終了し、
次回は最後の確定チェックポイントから再開する。
再開は同じ `run --output ...`。市場条件や `--resume` の指定は不要。

```bash
python3 "$RDA_MANAGER" run --output "$RDA_OUTPUT" --jobs 32
```

完了記録・条件・チェックサムが一致する区間は再計算しない。
未完了のチェックポイントは既存CLIの `--resume` で継続する。
最初のチェックポイント前に止まった管理対象ディレクトリは、
`initialization_backups/`へ退避してから再実行し、既存ファイルを消さない。
管理対象だった証拠がない既存ディレクトリや、壊れたチェックポイントは上書きせず拒否する。

ワーカー失敗時は新規起動を止め、他ワーカーにも保存を要求し、管理CLIは非ゼロで終了。
原因を直した後に同じ `run` で再試行する。失敗の無限自動再試行はしない。
条件・区間サイズ・Pythonドライバ・C++エンジンが変わった再開は拒否する。
計算途中にドライバの更新やエンジンの差し替えを行わない。

管理CLIの二重起動と統合中の起動はルートのロックで拒否する。
管理プロセスをSIGKILLした場合など、以前のワーカーがまだ生きていることがある。
その場合は区間ロックで検出し、重ねて起動せずエラーにする。
`status`で区間番号を確認し、`logs/*.attempt.json`のPID・ログを確認して、
以前のワーカーの終了を待つか停止させてから管理CLIを再開する。

## 統合と全件完了

通常は `run` の最後に自動統合する。手動実行は以下。

```bash
python3 "$RDA_MANAGER" merge --output "$RDA_OUTPUT"
```

- 全予定区間の完了、実験条件、チェックポイント・進捗・完了記録の対応を検査する。
- 代表数とラベル換算数を保持する。割合は元の件数から再計算する。
- ノード数・葉数等は合計し、最大ノード数・最大深さは最大値を取る。
- 初期違反数別の分類と、同じ葉に対する3安定性の真偽パターンも統合する。
- 各分類の代表例は、全区間で入力ID最小の例を残す。
- 各安定性の分類・存在数、探索木のノード・分岐数、葉のパターンの集計整合性を検査する。
- `--merge-profiles`指定時はヘッダ・IDの連続性・重み・件数を検査してストリーム連結し、
  入出力のCSVにSHA-256を記録する。CSV全体をメモリには載せない。
- 統合物は一時ディレクトリで作ってから `merged/` に置換する。
  未完了区間があれば完了済み統合物を作らない。同じ内容での再統合は再利用する。

`merged/completion.json`の `whole_domain_complete: true` が、市場の全代表を網羅した印。
`--start-id` / `--count` で一部分だけ計画した場合は `false` になる。
各区間の `whole_domain_complete` がfalseなのは正常。

統合された `search_stats.csv` の `elapsed_seconds` は各区間の累積稼働時間の合計で、
32並列実行全体の壁時計時間ではない。
`config.json`と`completion.json`の `elapsed_seconds_basis` に
`sum_chunk_active_seconds` と明記する。
管理側の累積稼働時間は `manager_status.json` の `manager_active_seconds` に保存し、
管理CLIを停止していた時間は含めない。強制終了時は最後の状態保存時点までの値となる。

旧 `merge_rda_exhaustive.py` は旧方式専用なので使わない。

## テスト

```bash
python3 tests/algorithms/test_rda_chunk_manager.py \
  build-release/experiments/universal_replacement/run_rda_exhaustive -v
```

小さい市場の一括実行とのCSV・証拠・任意の全入力CSVの一致、
32並列の動作、区間の端数・大きい整数、完了済み区間のスキップ、
途中ワーカーの再開・未確定CSV末尾の切り戻し、SIGTERMでの保存、
ワーカー失敗と再試行、初期化中断の退避、ロックと破損・条件変更の拒否を検証する。
