# 企業告白型RDA：2社・3人／4人の全列挙

個人・企業それぞれについて、未マッチ・空席を含むすべての厳密な順位を列挙する。
同僚選好は相互的な禁止ペア（`-INF`）／許容（0）の二値とし、全禁止ペア集合を列挙する。
禁止ペアなし、全ペア禁止、全相手を受け入れ不可とする選好も含める。

| 個人数 | 企業の定員 | 個人の選好 | 企業の選好 | 禁止ペア集合 | 入力総数 |
|---|---|---|---|---|---:|
| 3 | `[2,1]` | `6^3` | `24^2` | `2^3` | 995,328 |
| 4 | `[2,2]` | `6^4` | `120^2` | `2^6` | 1,194,393,600 |

未マッチ・空席より下にある相手同士の順位違いも別の入力として数える。
企業・個人のラベルの置換による同型な入力も省略しない。定員は上記に固定。
片側だけが相手を受け入れ不可とする選好も含む。

## RDAと集計の定義

既存の `recursive_firm_proposing_DA_algorithm` をそのまま呼ぶ。
各ノードで通常の企業告白型DAを最初から実行し、実際に同じ企業に所属した禁止ペアを調べる。
違反ペアの全選択肢と、その両端の削除対象をすべて再帰的に探索する。
枝刈り、同一状態の統合、安定な終点を発見した時点での打ち切りは行わない。
禁止ペアがなくなった枝が終点となる。重複する経路も数える。

UR安定性は、削除前の元の選好に対して既存の判定を使う。空席への追加逸脱も含む。
企業・個人間の選好スコアは「未マッチ・空席の順位 − 相手の順位」で、未マッチ・空席は0、
受け入れ可能な相手は正、受け入れ不可の相手は負。同順位はない。

入力ごとに以下の3分類を行う。

- `all_stable`：全終点が元の入力に対してUR安定。
- `mixed`：UR安定な終点と不安定な終点が混在。
- `none_stable`：全終点が不安定。

`none_stable` はRDAがUR安定な終点に到達できないという意味であり、
市場にUR安定マッチングが存在しないことを示すものではない。
経路数の比は、各分岐をランダムに選ぶアルゴリズムの成功確率とは異なる。
`nodes` は各入力でRDA内部から通常DAを呼んだ回数の合計。

## ビルド

`MatchingPrograms` を作業ディレクトリとする。

```sh
cmake -S . -B /tmp/rda-exhaustive-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rda-exhaustive-build --target run_rda_exhaustive test_rda_exhaustive -j4
ctest --test-dir /tmp/rda-exhaustive-build -R RDAExhaustive --output-on-failure
python3 tests/algorithms/test_rda_exhaustive_cli.py \
  /tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive
```

GoogleTestがローカルに保存済みなら、ネットワークからの取得を避けて構成できる。

```sh
cmake -S . -B /tmp/rda-exhaustive-build -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

## 実行

件数の確認のみなら `--dry-run` を使う。この場合は出力ディレクトリを作らない。

```sh
/tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive \
  --workers 3 --dry-run
/tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive \
  --workers 4 --dry-run
```

全列挙する場合は、存在しない出力ディレクトリを指定する。

```sh
/tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive \
  --workers 3 --output experiments/universal_replacement/outputs/exhaustive_2x3

/tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive \
  --workers 4 --output experiments/universal_replacement/outputs/exhaustive_2x4
```

入力IDは0始まり。各個人の順位、企業f1・f2の順位、禁止ペアマスクの順に並ぶ。
順位の選択肢は、相手のラベル `1,2,...` と未マッチ・空席のラベル `0` の辞書順の順列。
禁止ペアマスクが最も速く変わり、次にf2、f1、最後の個人、…、最初の個人が変わる。
マスクの下位ビットから `(w1,w2),(w1,w3),...,(w2,w3),...` の順。
IDから入力を直接復元するので、開始位置までの列挙を実行する必要はない。

```sh
# 4人市場の先頭10万入力。全件の代わりに小規模な試行ができる。
/tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive \
  --workers 4 --count 100000 \
  --output experiments/universal_replacement/outputs/exhaustive_2x4_part0

# 続く10万入力。区間は [start-id, start-id + count)。
/tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive \
  --workers 4 --start-id 100000 --count 100000 \
  --output experiments/universal_replacement/outputs/exhaustive_2x4_part1

# 保存した例を1件だけ再現し、全入力別CSVも保存する。
/tmp/rda-exhaustive-build/experiments/universal_replacement/run_rda_exhaustive \
  --workers 4 --start-id 1194393599 --count 1 --write-profiles \
  --output experiments/universal_replacement/outputs/exhaustive_2x4_last
```

`--count` を省略すると開始IDから末尾まで実行する。範囲外のIDや件数はエラーとする。
互いに重複しない区間を別プロセスで実行すれば並列化できる。各プロセスで出力先を分ける。
先頭区間は未マッチ・空席を最上位に置く選好に偏るため、先頭の試行速度を全体の速度とみなさない。

## 出力

| ファイル | 内容 |
|---|---|
| `config.json` | 市場・定員・列挙規則・全件数・実行区間・RDAの条件 |
| `summary.csv` | 入力全体、および禁止ペア数×初回DA違反ペア数別の3分類と探索量 |
| `examples.jsonl` | 禁止ペア数×初期違反数×3分類ごとの最初の代表例。全順位・スコア・安定／不安定の証拠終点を保存 |
| `progress.json` | 最後に保存した処理件数、次の入力ID、経過秒数 |
| `completion.json` | 指定区間の全件完了、全市場を網羅したか、処理件数、DA呼び出し数、経過秒数 |
| `profiles.csv` | `--write-profiles` 指定時のみ。各入力のID・分類・探索量・終点数 |

`summary.csv` の `scope=overall` は全体の1行、`by_conflicts_and_forbidden` は内訳。
全体と内訳を足し合わせない。`initial_conflicts=0` は初回DAから禁止ペアがなかった入力。
違反の解消が必要だった入力だけを見る場合は、内訳の `initial_conflicts>0` を集計する。
`distinct_matchings` と `distinct_stable_matchings` は入力ごとに重複を除いた終点数の合計であり、
異なる入力間で同じ割当を統合した件数ではない。

代表例の個人・企業ラベルは1始まり、順位中の0は未マッチ・空席。
`assignment` の各要素は個人w1,w2,…の所属企業で、0は未マッチ。
証拠終点には順序付きの削除履歴と、元の入力に対するブロッキングペアを記録する。

進捗と集計の途中保存は既定で10,000入力ごと。`--progress-every N` で変更できる。
途中で停止した実行には `completion.json` がないため、完了済み全列挙として扱わない。
同じ出力先での自動再開は行わない。区間を小さく分ければ、中断時に再実行する範囲を抑えられる。
約11.9億入力のCSVは大きくなるため、既定では全入力別の行や全探索木を保存しない。
代表例と集計だけなら、全体の実行件数に比例するメモリは必要ない。

## 分割結果の統合

Python標準ライブラリだけで統合できる。完了記録、実験条件、区間の重複、集計の整合性を検査する。

```sh
python3 experiments/universal_replacement/merge_rda_exhaustive.py \
  --output experiments/universal_replacement/outputs/exhaustive_2x4_merged \
  experiments/universal_replacement/outputs/exhaustive_2x4_part0 \
  experiments/universal_replacement/outputs/exhaustive_2x4_part1
```

全市場を網羅した結果として統合するときは `--require-full` を付ける。
開始・終了の不足や区間間の抜けがあればエラーになる。
統合済み結果をさらに統合することもできる。入力別CSVは統合せず、代表例は各分類で最小のIDを残す。
統合結果の経過秒数は元の各実行の秒数の合計であり、並列実行の壁時計時間ではない。
