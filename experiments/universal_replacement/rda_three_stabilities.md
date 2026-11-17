# 企業告白型RDA：対称性を除いた全列挙と3種類の安定性

企業数・個人数・企業の定員・禁止ペア数を指定し、代表入力の全列挙、
RDAの全分岐探索、全葉の3安定性判定、市場全体での安定解の存在調査を行う。
1実行の関連ファイルを、指定した1つの出力ディレクトリにまとめる。

大規模実験の区間生成・並列実行・再開・統合には、
[分割実行管理プログラム](rda_chunk_manager.md)を使う。

従来の `--workers 3|4` だけの実行は従来の条件・出力形式を保つ。
新方式では `--firms`、`--workers`、`--capacities`、`--forbidden-pairs` を指定する。

## ビルドと実行

リポジトリを作業ディレクトリにする。Python 3標準ライブラリとC++17を使う。
Pythonは入力列挙・任意精度整数の集計・保存・再開を担当する。
常駐するC++エンジンが既存RDAと既存Matchingの判定を呼ぶ。
入力ごとにC++プロセスを起動し直すことはしない。

```sh
cmake -S . -B /tmp/rda-three-stabilities-build -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/rda-three-stabilities-build --target run_rda_exhaustive -j4

/tmp/rda-three-stabilities-build/experiments/universal_replacement/run_rda_exhaustive \
  --firms 2 --workers 2 --capacities 1,2 --forbidden-pairs 1 \
  --output experiments/universal_replacement/outputs/three_stabilities_2x2_k1

/tmp/rda-three-stabilities-build/experiments/universal_replacement/run_rda_exhaustive \
  --firms 3 --workers 4 --capacities 1,1,2 --forbidden-pairs 2 --dry-run
```

GoogleTestの指定はローカルにソースがある場合の例。補助エンジン
`rda_stability_engine`も自動ビルドする。CLIはソース内のPythonドライバの
絶対パスを参照するため、リポジトリを移動した場合は再構成・再ビルドする。

| 引数 | 意味 |
|---|---|
| `--firms N` | 企業数。現在は2または3 |
| `--workers M` | 個人数。現在は2〜5 |
| `--capacities C,C,...` | 企業ラベル順の定員。各値は0〜M |
| `--forbidden-pairs K` | 無向の禁止ペア数。1〜M-1 |
| `--output DIRECTORY` | 新しい出力ディレクトリ |
| `--start-id ID` | 0始まりの代表入力ID。既定0 |
| `--count N` | 処理代表数。既定は開始IDから末尾まで |
| `--progress-every N` | 途中保存間隔。既定1,000代表 |
| `--write-profiles` | 全代表入力別のCSVを追加保存 |
| `--dry-run` | 件数と区間をJSON表示。出力ディレクトリは作らない |
| `--stop-after N` | 今回の起動でN代表を処理したら保存して停止 |
| `--resume DIRECTORY` | 保存した区間の続きから再開 |

直接代表を生成する方式のため、少なくとも1社は定員が固有である必要がある。
合意した4市場の定員 `[1,2]`、`[1,2]`、`[1,1,2]`、`[1,2,2]` はすべて対応。
企業ラベル順が違う定員にも対応。全企業同一定員の市場は、自己同型を
別途扱う必要があるため明示的にエラーとする。
禁止ペア数は1回に1つ指定し、別のKは別の出力先で実行する。

## 実験・代表入力IDの定義

- 個人は全企業と未マッチ、企業は全個人と空席を含む厳密順位を持つ。
  受け入れ不可の相手同士の順位も区別する。
- スコアは「未マッチ・空席の順位 − 相手の順位」。未マッチ・空席は0。
- 禁止ペアは全企業で共通。両方向を `-INF`、他の同僚評価を0にする。
- 定員が固有の企業のうち最小ラベルを基準に、その企業の順位中の個人名を
  w1,w2,...の順に固定する。空席の全位置を残す。
- 同一定員企業はw1の厳密順位でラベルが昇順に現れるよう固定する。
  禁止ペアは正規化後の名前で全配置を列挙する。
- この条件では非自明な自己同型はなく、全代表の重みは一律
  `m! × 同一定員企業の各グループの人数の階乗の積`。
- 個人の順位、企業の順位、禁止ペア集合の順に混合基数でID化。
  禁止ペア集合が最も速く変化する。通常の順位は0を含む辞書順、
  基準企業は空席の挿入位置0〜mの順、禁止集合はペアの辞書順組合せ。
  開始IDまでの空走査なしで直接復元できる。
- RDAは全違反ペア・両削除対象を探索する。枝刈り・状態統合・
  安定解発見による打ち切りはしない。禁止ペアがなくなったノードを葉とする。
- 全葉を削除前の元の入力に対して `ur`、`weak`、`stability` で独立判定。
  URは空席への追加逸脱を含む。
- 別途、未マッチを含む定員内の全割当を列挙する。禁止ペアと個人合理性を
  満たさない割当は共通の必要条件で除外し、残りを既存3判定にかける。
  存在発見後も列挙を続け、各定義の安定マッチング数を数える。

## 出力ディレクトリ

```text
three_stabilities_2x2_k1/
├── config.json
├── summary.csv
├── profile_classes.csv
├── leaf_patterns.csv
├── search_stats.csv
├── examples.jsonl
├── profiles.csv          # --write-profiles指定時のみ
├── checkpoint.json
├── progress.json
├── completion.json       # 指定区間が完了したときだけ
└── .run.lock             # 同じ出力先への同時書き込みを防ぐ
```

CSVはUTF-8、ヘッダあり。件数は指数表記を使わない任意精度整数。
JSONも件数を整数で保存する。巨大な件数はExcelやJavaScriptの数値型で
丸められることがあるため、整数として読み込む。Python標準のcsv/jsonでは正確に読める。

### summary.csv

`stability` × `count_basis` の6行。安定性は `ur`、`weak`、`stability`、
集計基準は `representative`、`labeled`。

| 列 | 意味 |
|---|---|
| `inputs` | 処理入力数 |
| `stable_exists`、`stable_not_exists` | 市場に安定解が存在する／しない入力数 |
| `rda_all_stable` | 全葉が安定な入力数 |
| `rda_mixed` | 安定・不安定な葉が混在する入力数 |
| `rda_none_stable` | 安定な葉がない入力数 |
| `exists_but_unreached` | 安定解が存在するがRDAの全葉が不安定な入力数 |
| `stable_leaves` | 安定な葉への経路数の合計 |
| `distinct_stable_matchings` | 入力ごとに重複を除いた安定終点数の合計 |
| `stable_solution_counts` | 市場全体の安定マッチング数の、入力ごとの合計 |
| `reach_rate_given_exists` | `(all_stable + mixed) / stable_exists`。分母0なら空欄 |

別入力間の同じ割当は統合しない。`none_stable`は安定解不存在を意味しない。
6行をまとめて加算しない。代表と換算は同じ入力を異なる基準で数えている。

### profile_classes.csv

1行は初期違反数と、3種類それぞれの存在真偽・RDA分類の組合せ。
`initial_conflicts`、`{ur,weak,stability}_exists`、
`{ur,weak,stability}_category`、`representative_inputs`、`labeled_inputs`。
真偽は0/1。該当入力0件の組合せは行を出さない。
各行は重複せず、代表数を全行合計すると処理代表数になる。
`initial_conflicts > 0`で違反解消が必要だった入力に限定できる。

### leaf_patterns.csv

1行は `initial_conflicts` と、同じ葉の `ur_stable`、`weakly_stable`、`stable` の組合せ。
次の3量を代表／ラベル換算の両方で保存する。

- `representative_inputs_with_pattern`、`labeled_inputs_with_pattern`
- `leaf_paths_representative`、`leaf_paths_labeled`
- `distinct_matchings_representative`、`distinct_matchings_labeled`

1入力が複数の真偽パターンを持つため、入力数列の全行合計は処理入力数と
一致するとは限らない。経路数の比は乱択RDAの成功確率ではない。

### search_stats.csv

実際に処理した代表についての1行。`representative_inputs`、`labeled_inputs`と、
`nodes`、`branches`、`leaves`、`distinct_matchings`、`assignment_candidates`、
`evaluated_assignments`、`max_nodes`、`max_deletions`、`elapsed_seconds`、
`rda_seconds`、`existence_seconds`を保存する。
`assignment_candidates`は定員内の全割当数、`evaluated_assignments`は
禁止ペア・個人合理性の除外後に3判定にかけた割当数。
探索量は換算値ではなく実際に計算した代表の量。
`rda_seconds`はC++内のRDAと全葉の判定、`existence_seconds`はC++内の存在調査。
総時間は列挙・通信・集計・途中保存等も含む累積稼働時間で、再開までの休止は含めない。

### examples.jsonl

`profile_classes.csv`の各組合せにつき、処理範囲内でID最小の入力を1件保存。
元の全順位・スコア・定員・禁止ペア・重みと、次の結果を保存する。

- 3種類の安定マッチング数と、存在すれば各定義の証拠マッチング。
- 各定義の安定／不安定な証拠葉を各1件。兼用できる葉は重複保存しない。
- 証拠葉の3判定と各定義のブロッキングペア。
  general stabilityにはブロッキングを示す残留所属者集合も付ける。
- 葉までの順序付き経路。各削除の企業・理由の禁止ペア・削除側・親と子の割当。
- 不存在の場合は証拠マッチングをnullとし、`existence_search_complete`と調査件数を残す。

ラベルは1始まり、割当と順位の0は未マッチ・空席。
配列内の安定性の順序は `[ur, weak, stability]`。
`conflict_index`と`leaf_id`は0始まり。

### profiles.csv（オプション）

1行1代表。ID・重み・初期違反数、3種類の存在真偽・RDA分類・安定葉数・
異なる安定終点数・市場全体の安定解数と探索量を保存する。
全選好行列や全探索木は入れない。入力IDから元の入力を復元し、
`--start-id ID --count 1`で再実行できる。

## 再開と区間実行

```sh
# 新しい出力先を指定し、先頭1,000代表で保存して停止。
/tmp/rda-three-stabilities-build/experiments/universal_replacement/run_rda_exhaustive \
  --firms 2 --workers 3 --capacities 1,2 --forbidden-pairs 1 \
  --stop-after 1000 --write-profiles --output /tmp/rda_paused

# 条件と区間は保存したものを使う。
/tmp/rda-three-stabilities-build/experiments/universal_replacement/run_rda_exhaustive \
  --resume /tmp/rda_paused
```

SIGINT / SIGTERMでは現在の入力を処理した後に保存して終了する。
強制終了等では最後のチェックポイントまで戻る。
`checkpoint.json`はチェックサム付きの正式な再開状態で、集計・代表例・次ID・
全入力CSVの確定バイト位置を含む。再開時は未確定のCSV末尾を切り戻し、
集計ファイルを再生成するため、途中保存中の停止で二重計上しない。
ファイル単位の置換はatomic。停止直後に集計ファイルの世代が混在して見える場合は、
再開して再生成する。条件・実装・エンジンが変わった再開は拒否する。
同じ出力先への同時書き込みはロックで拒否する。

`completion.json`の `complete` は指定区間の完了、
`whole_domain_complete`は全領域を単独実行で網羅したかを表す。
`--start-id` / `--count`で重ならない区間を別出力先へ分割できる。
従来の `merge_rda_exhaustive.py` は旧方式専用で、新方式の統合には使わない。
新方式の分割結果は[管理CLI](rda_chunk_manager.md)で検査・自動統合できる。

## 検証

```sh
python3 tests/algorithms/test_rda_three_stabilities_cli.py \
  /tmp/rda-three-stabilities-build/experiments/universal_replacement/run_rda_exhaustive -v
```

2x2の全代表と他市場の固定seed標本を、独立PythonモデルのDA・全分岐木・
3安定性・全割当の安定解数と比較し、証拠経路の全DAも再実行する。
2x2の全ラベル付き入力と代表集合、見積件数、全件／区間の完了区別、
途中再開と一括実行の集計・証拠の一致、破損状態・条件変更・同時書き込みの拒否、
SIGTERMによる保存と再開も検証する。
